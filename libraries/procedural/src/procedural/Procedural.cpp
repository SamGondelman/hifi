//
//  Created by Bradley Austin Davis on 2015/09/05
//  Copyright 2013-2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Procedural.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QDateTime>

#include <gpu/Batch.h>
#include <SharedUtil.h>
#include <NumericalConstants.h>
#include <GLMHelpers.h>
#include <NetworkingConstants.h>
#include <shaders/Shaders.h>

#include "ShaderConstants.h"
#include "Logging.h"

Q_LOGGING_CATEGORY(proceduralLog, "hifi.gpu.procedural")

// Userdata parsing constants
static const QString URL_KEY = "shaderUrl";
static const QString VERTEX_URL_KEY = "vertexShaderUrl";
static const QString FRAGMENT_URL_KEY = "fragmentShaderUrl";
static const QString VERSION_KEY = "version";
static const QString UNIFORMS_KEY = "uniforms";
static const QString CHANNELS_KEY = "channels";

// Shader replace strings
static const std::string PROCEDURAL_BLOCK = "//PROCEDURAL_BLOCK";
static const std::string PROCEDURAL_VERSION = "//PROCEDURAL_VERSION";

bool operator==(const ProceduralData& a, const ProceduralData& b) {
    return (
        (a.version == b.version) &&
        (a.fragmentShaderUrl == b.fragmentShaderUrl) &&
        (a.vertexShaderUrl == b.vertexShaderUrl) &&
        (a.uniforms == b.uniforms) &&
        (a.channels == b.channels));
}

QJsonValue ProceduralData::getProceduralData(const QString& proceduralJson) {
    if (proceduralJson.isEmpty()) {
        return QJsonValue();
    }

    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(proceduralJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return QJsonValue();
    }

    return doc.object();
}

ProceduralData ProceduralData::parse(const QString& userDataJson) {
    ProceduralData result;
    result.parse(getProceduralData(userDataJson).toObject());
    return result;
}

void ProceduralData::parse(const QJsonObject& proceduralData) {
    if (proceduralData.isEmpty()) {
        return;
    }

    {
        auto versionJson = proceduralData[VERSION_KEY];
        if (versionJson.isDouble()) {
            version = (uint8_t)(floor(versionJson.toDouble()));
            // invalid version
            if (!(version == 1 || version == 2 || version == 3)) {
                return;
            }
        } else {
            // All unversioned shaders default to V1
            version = 1;
        }
    }

    {  // Fragment shader URL (either fragmentShaderUrl or shaderUrl)
        auto rawShaderUrl = proceduralData[FRAGMENT_URL_KEY].toString();
        fragmentShaderUrl = DependencyManager::get<ResourceManager>()->normalizeURL(rawShaderUrl);

        if (fragmentShaderUrl.isEmpty()) {
            rawShaderUrl = proceduralData[URL_KEY].toString();
            fragmentShaderUrl = DependencyManager::get<ResourceManager>()->normalizeURL(rawShaderUrl);
        }
    }

    {  // Vertex shader URL
        auto rawShaderUrl = proceduralData[VERTEX_URL_KEY].toString();
        vertexShaderUrl = DependencyManager::get<ResourceManager>()->normalizeURL(rawShaderUrl);
    }

    uniforms = proceduralData[UNIFORMS_KEY].toObject();
    channels = proceduralData[CHANNELS_KEY].toArray();
}

std::function<void(gpu::StatePointer)> Procedural::opaqueStencil = [](gpu::StatePointer state) {};
std::function<void(gpu::StatePointer)> Procedural::transparentStencil = [](gpu::StatePointer state) {};

Procedural::Procedural() {
    _opaqueState->setCullMode(gpu::State::CULL_BACK);
    _opaqueState->setDepthTest(true, true, gpu::LESS_EQUAL);
    _opaqueState->setBlendFunction(false, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD,
                                   gpu::State::INV_SRC_ALPHA, gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD,
                                   gpu::State::ONE);
    opaqueStencil(_opaqueState);

    _transparentState->setCullMode(gpu::State::CULL_BACK);
    _transparentState->setDepthTest(true, true, gpu::LESS_EQUAL);
    _transparentState->setBlendFunction(true, gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD,
                                        gpu::State::INV_SRC_ALPHA, gpu::State::FACTOR_ALPHA,
                                        gpu::State::BLEND_OP_ADD, gpu::State::ONE);
    transparentStencil(_transparentState);

    _standardInputsBuffer = std::make_shared<gpu::Buffer>(sizeof(StandardInputs), nullptr);
}

Procedural::Procedural(const Procedural& other) {
    _vertexSource = other._vertexSource;
    _vertexSourceSkin = other._vertexSourceSkin;
    _vertexSourceSkinDQ = other._vertexSourceSkinDQ;
    _opaqueFragmentSource = other._opaqueFragmentSource;
    _transparentFragmentSource = other._transparentFragmentSource;
    _opaqueState = other._opaqueState;
    _transparentState = other._transparentState;

    _data = other._data;

    _enabled = other._enabled;

    _vertexShaderPath = other._vertexShaderPath;
    _networkVertexShader = other._networkVertexShader;
    _fragmentShaderPath = other._fragmentShaderPath;
    _networkFragmentShader = other._networkFragmentShader;
}

void Procedural::setProceduralData(const ProceduralData& proceduralData) {
    std::lock_guard<std::mutex> lock(_lock);
    if (proceduralData == _data) {
        return;
    }

    _enabled = false;

    if (proceduralData.version != _data.version) {
        _data.version = proceduralData.version;
        _shaderDirty = true;
    }

    if (proceduralData.uniforms != _data.uniforms) {
        // If the uniform keys changed, we need to recreate the whole shader to handle the reflection
        if (proceduralData.uniforms.keys() != _data.uniforms.keys()) {
            _shaderDirty = true;
        }
        _data.uniforms = proceduralData.uniforms;
        _uniformsDirty = true;
    }

    if (proceduralData.channels != _data.channels) {
        _data.channels = proceduralData.channels;
        // Must happen on the main thread
        auto textureCache = DependencyManager::get<TextureCache>();
        size_t channelCount = std::min(MAX_PROCEDURAL_TEXTURE_CHANNELS, (size_t)proceduralData.channels.size());
        size_t channel;
        for (channel = 0; channel < MAX_PROCEDURAL_TEXTURE_CHANNELS; ++channel) {
            if (channel < channelCount) {
                QString url = proceduralData.channels.at((int)channel).toString();
                _channels[channel] = textureCache->getTexture(QUrl(url));
            } else {
                // Release those textures no longer in use
                _channels[channel] = textureCache->getTexture(QUrl());
            }
        }
    }

    if (proceduralData.fragmentShaderUrl != _data.fragmentShaderUrl) {
        _data.fragmentShaderUrl = proceduralData.fragmentShaderUrl;
        _shaderDirty = true;
        const auto& shaderUrl = _data.fragmentShaderUrl;
        _networkFragmentShader.reset();
        _fragmentShaderPath.clear();

        if (!shaderUrl.isValid()) {
            qCWarning(proceduralLog) << "Invalid fragment shader URL: " << shaderUrl;
            return;
        }

        if (shaderUrl.isLocalFile()) {
            if (!QFileInfo(shaderUrl.toLocalFile()).exists()) {
                qCWarning(proceduralLog) << "Invalid fragment shader URL, missing local file: " << shaderUrl;
                return;
            }
            _fragmentShaderPath = shaderUrl.toLocalFile();
        } else if (shaderUrl.scheme() == URL_SCHEME_QRC) {
            _fragmentShaderPath = ":" + shaderUrl.path();
        } else {
            _networkFragmentShader = ShaderCache::instance().getShader(shaderUrl);
        }
    }

    /* Procedural vertex shaders are disabled for now.  Add to JSDoc when enabled
    if (proceduralData.vertexShaderUrl != _data.vertexShaderUrl) {
        _data.vertexShaderUrl = proceduralData.vertexShaderUrl;
        _shaderDirty = true;
        const auto& shaderUrl = _data.vertexShaderUrl;
        _networkVertexShader.reset();
        _vertexShaderPath.clear();

        if (!shaderUrl.isValid()) {
            qCWarning(proceduralLog) << "Invalid vertex shader URL: " << shaderUrl;
            return;
        }

        if (shaderUrl.isLocalFile()) {
            if (!QFileInfo(shaderUrl.toLocalFile()).exists()) {
                qCWarning(proceduralLog) << "Invalid vertex shader URL, missing local file: " << shaderUrl;
                return;
            }
            _vertexShaderPath = shaderUrl.toLocalFile();
        } else if (shaderUrl.scheme() == URL_SCHEME_QRC) {
            _vertexShaderPath = ":" + shaderUrl.path();
        } else {
            _networkVertexShader = ShaderCache::instance().getShader(shaderUrl);
        }
    }
    */

    _enabled = true;
}

bool Procedural::isReady() const {
#if defined(USE_GLES)
    return false;
#endif

    std::lock_guard<std::mutex> lock(_lock);
    if (!_enabled) {
        return false;
    }

    if (!_hasStartedFade) {
        _fadeStartTime = usecTimestampNow();
    }

    bool hasFragmentShader = !_fragmentShaderPath.isEmpty() || _networkFragmentShader;
    bool fragmentShaderLoaded = !_fragmentShaderPath.isEmpty() || (_networkFragmentShader && _networkFragmentShader->isLoaded());
    bool hasVertexShader = !_vertexShaderPath.isEmpty() || _networkVertexShader;
    bool vertexShaderLoaded = !_vertexShaderPath.isEmpty() || (_networkVertexShader && _networkVertexShader->isLoaded());

    // We need to have at least one shader, and whichever ones we have need to be loaded
    if ((!hasFragmentShader && !hasVertexShader) || (hasFragmentShader && !fragmentShaderLoaded) || (hasVertexShader && !vertexShaderLoaded)) {
        return false;
    }

    // Do we have textures, and if so, are they loaded?
    for (size_t i = 0; i < MAX_PROCEDURAL_TEXTURE_CHANNELS; ++i) {
        if (_channels[i] && !_channels[i]->isLoaded()) {
            return false;
        }
    }

    if (!_hasStartedFade) {
        _hasStartedFade = true;
        _isFading = true;
    }

    return true;
}

void Procedural::prepare(gpu::Batch& batch,
                         const glm::vec3& position,
                         const glm::vec3& size,
                         const glm::quat& orientation,
                         const ProceduralProgramKey key) {
    std::lock_guard<std::mutex> lock(_lock);

    _entityDimensions = size;
    _entityPosition = position;
    _entityOrientation = glm::mat3_cast(orientation);
    if (!_fragmentShaderPath.isEmpty()) {
        auto lastModified = (quint64)QFileInfo(_fragmentShaderPath).lastModified().toMSecsSinceEpoch();
        if (lastModified > _fragmentShaderModified) {
            QFile file(_fragmentShaderPath);
            file.open(QIODevice::ReadOnly);
            _fragmentShaderSource = QTextStream(&file).readAll();
            _shaderDirty = true;
            _fragmentShaderModified = lastModified;
        }
    } else if (_networkFragmentShader && _networkFragmentShader->isLoaded()) {
        _fragmentShaderSource = _networkFragmentShader->_source;
    }

    if (!_vertexShaderPath.isEmpty()) {
        auto lastModified = (quint64)QFileInfo(_vertexShaderPath).lastModified().toMSecsSinceEpoch();
        if (lastModified > _vertexShaderModified) {
            QFile file(_vertexShaderPath);
            file.open(QIODevice::ReadOnly);
            _vertexShaderSource = QTextStream(&file).readAll();
            _shaderDirty = true;
            _vertexShaderModified = lastModified;
        }
    } else if (_networkVertexShader && _networkVertexShader->isLoaded()) {
        _vertexShaderSource = _networkVertexShader->_source;
    }

    auto pipeline = _proceduralPipelines.find(key);
    bool recompiledShader = false;
    if (pipeline == _proceduralPipelines.end() || _shaderDirty) {
        gpu::Shader::Source vertexSource;
        if (key.isSkinnedDQ()) {
            vertexSource = _vertexSourceSkinDQ;
        } else if (key.isSkinned()) {
            vertexSource = _vertexSourceSkin;
        } else {
            vertexSource = _vertexSource;
        }

        gpu::Shader::Source fragmentSource;
        if (key.isTransparent()) {
            fragmentSource = _transparentFragmentSource;
        } else {
            fragmentSource = _opaqueFragmentSource;
        }

        // Build the fragment shader
        vertexSource.replacements.clear();
        fragmentSource.replacements.clear();
        if (_data.version == 1) {
            vertexSource.replacements[PROCEDURAL_VERSION] = "#define PROCEDURAL_V1 1";
            fragmentSource.replacements[PROCEDURAL_VERSION] = "#define PROCEDURAL_V1 1";
        } else if (_data.version == 2) {
            vertexSource.replacements[PROCEDURAL_VERSION] = "#define PROCEDURAL_V2 1";
            fragmentSource.replacements[PROCEDURAL_VERSION] = "#define PROCEDURAL_V2 1";
        } else if (_data.version == 3) {
            vertexSource.replacements[PROCEDURAL_VERSION] = "#define PROCEDURAL_V3 1";
            fragmentSource.replacements[PROCEDURAL_VERSION] = "#define PROCEDURAL_V3 1";
        }
        vertexSource.replacements[PROCEDURAL_BLOCK] = _vertexShaderSource.toStdString();
        fragmentSource.replacements[PROCEDURAL_BLOCK] = _fragmentShaderSource.toStdString();

        // Set any userdata specified uniforms
        int customSlot = procedural::slot::uniform::Custom;
        for (const auto& key : _data.uniforms.keys()) {
            std::string uniformName = key.toLocal8Bit().data();
            vertexSource.reflection.uniforms[uniformName] = customSlot;
            fragmentSource.reflection.uniforms[uniformName] = customSlot;
            ++customSlot;
        }

        // Leave this here for debugging
        //qCDebug(proceduralLog) << "VertexShader:\n" << vertexSource.getSource(shader::Dialect::glsl450, shader::Variant::Mono).c_str();
        //qCDebug(proceduralLog) << "FragmnetShader:\n" << fragmentSource.getSource(shader::Dialect::glsl450, shader::Variant::Mono).c_str();

        gpu::ShaderPointer vertexShader = gpu::Shader::createVertex(vertexSource);
        gpu::ShaderPointer fragmentShader = gpu::Shader::createPixel(fragmentSource);
        gpu::ShaderPointer program = gpu::Shader::createProgram(vertexShader, fragmentShader);

        _proceduralPrograms[key] = program;
        _proceduralPipelines[key] = gpu::Pipeline::create(program, key.isTransparent() ? _transparentState : _opaqueState);

        _start = usecTimestampNow();
        _frameCount = 0;
        recompiledShader = true;
    }

    batch.setPipeline(recompiledShader ? _proceduralPipelines[key] : *pipeline);

    bool recreateUniforms = _shaderDirty || _uniformsDirty || recompiledShader || _prevKey != key;
    if (recreateUniforms) {
        setupUniforms();
    }

    _prevKey = key;
    _shaderDirty = _uniformsDirty = false;

    for (auto lambda : _uniforms) {
        lambda(batch);
    }

    static gpu::Sampler sampler;
    static std::once_flag once;
    std::call_once(once, [&] {
        gpu::Sampler::Desc desc;
        desc._filter = gpu::Sampler::FILTER_MIN_MAG_MIP_LINEAR;
    });

    for (size_t i = 0; i < MAX_PROCEDURAL_TEXTURE_CHANNELS; ++i) {
        if (_channels[i] && _channels[i]->isLoaded()) {
            auto gpuTexture = _channels[i]->getGPUTexture();
            if (gpuTexture) {
                gpuTexture->setSampler(sampler);
                gpuTexture->setAutoGenerateMips(true);
            }
            batch.setResourceTexture((gpu::uint32)i, gpuTexture);
        }
    }
}

void Procedural::setupUniforms() {
    _uniforms.clear();
    auto customUniformCount = _data.uniforms.keys().size();
    // Set any userdata specified uniforms
    for (int i = 0; i < customUniformCount; ++i) {
        int slot = procedural::slot::uniform::Custom + i;
        QString key = _data.uniforms.keys().at(i);
        std::string uniformName = key.toLocal8Bit().data();
        QJsonValue value = _data.uniforms[key];
        if (value.isDouble()) {
            float v = value.toDouble();
            _uniforms.push_back([slot, v](gpu::Batch& batch) { batch._glUniform1f(slot, v); });
        } else if (value.isArray()) {
            auto valueArray = value.toArray();
            switch (valueArray.size()) {
                case 0:
                    break;

                case 1: {
                    float v = valueArray[0].toDouble();
                    _uniforms.push_back([slot, v](gpu::Batch& batch) { batch._glUniform1f(slot, v); });
                    break;
                }

                case 2: {
                    glm::vec2 v{ valueArray[0].toDouble(), valueArray[1].toDouble() };
                    _uniforms.push_back([slot, v](gpu::Batch& batch) { batch._glUniform2f(slot, v.x, v.y); });
                    break;
                }

                case 3: {
                    glm::vec3 v{
                        valueArray[0].toDouble(),
                        valueArray[1].toDouble(),
                        valueArray[2].toDouble(),
                    };
                    _uniforms.push_back([slot, v](gpu::Batch& batch) { batch._glUniform3f(slot, v.x, v.y, v.z); });
                    break;
                }

                default:
                case 4: {
                    glm::vec4 v{
                        valueArray[0].toDouble(),
                        valueArray[1].toDouble(),
                        valueArray[2].toDouble(),
                        valueArray[3].toDouble(),
                    };
                    _uniforms.push_back([slot, v](gpu::Batch& batch) { batch._glUniform4f(slot, v.x, v.y, v.z, v.w); });
                    break;
                }
            }
        }
    }

    _uniforms.push_back([=](gpu::Batch& batch) {
        _standardInputs.position = vec4(_entityPosition, 1.0f);
        // Minimize floating point error by doing an integer division to milliseconds, before the floating point division to seconds
        _standardInputs.time = (float)((usecTimestampNow() - _start) / USECS_PER_MSEC) / MSECS_PER_SECOND;

        // Date
        {
            QDateTime now = QDateTime::currentDateTimeUtc();
            QDate date = now.date();
            QTime time = now.time();
            _standardInputs.date.x = date.year();
            // Shadertoy month is 0 based
            _standardInputs.date.y = date.month() - 1;
            // But not the day... go figure
            _standardInputs.date.z = date.day();
            float fractSeconds = (time.msec() / 1000.0f);

            _standardInputs.date.w = (time.hour() * 3600) + (time.minute() * 60) + time.second() + fractSeconds;
        }

        _standardInputs.scale = vec4(_entityDimensions, 1.0f);
        _standardInputs.frameCount = ++_frameCount;
        _standardInputs.orientation = mat4(_entityOrientation);

        for (size_t i = 0; i < MAX_PROCEDURAL_TEXTURE_CHANNELS; ++i) {
            if (_channels[i]) {
                _standardInputs.resolution[i] = vec4(_channels[i]->getWidth(), _channels[i]->getHeight(), 1.0f, 1.0f);
            } else {
                _standardInputs.resolution[i] = vec4(1.0f);
            }
        }

        _standardInputsBuffer->setSubData(0, _standardInputs);
        batch.setUniformBuffer(0, _standardInputsBuffer, 0, sizeof(StandardInputs));
    });
}

glm::vec4 Procedural::getColor(const glm::vec4& entityColor) const {
    if (_data.version == 1) {
        return glm::vec4(1);
    }
    return entityColor;
}

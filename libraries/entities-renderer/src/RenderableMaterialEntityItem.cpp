//
//  Created by Sam Gondelman on 1/18/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableMaterialEntityItem.h"

#include "RenderPipelines.h"
#include "GeometryCache.h"

using namespace render;
using namespace render::entities;

bool MaterialEntityRenderer::needsRenderUpdate() const {
    if (_retryApply) {
        return true;
    }
    return Parent::needsRenderUpdate();
}

bool MaterialEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (entity->getMaterialURL() != _materialURL) {
        return true;
    }
    if (entity->getParentID() != _parentID) {
        return true;
    }
    if (entity->getMaterialMappingPos() != _materialMappingPos || entity->getMaterialMappingScale() != _materialMappingScale || entity->getMaterialMappingRot() != _materialMappingRot) {
        return true;
    }
    if (entity->getPriority() != _priority) {
        return true;
    }
    if (entity->getParentMaterialName() != _parentMaterialName) {
        return true;
    }
    if (entity->getMaterialData() != _materialData) {
        return true;
    }
    return false;
}

void MaterialEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    withWriteLock([&] {
        bool materialNeedsUpdate = false;
        if (entity->getMaterialURL() != _materialURL) {
            if (!materialNeedsUpdate) {
                deleteMaterial();
            }
            _materialURL = entity->getMaterialURL();
            if (_materialURL.startsWith("materialData")) {
                materialNeedsUpdate = true;
            }
            setMaterialURL(_materialURL);
        }
        if (entity->getParentID() != _parentID) {
            if (!materialNeedsUpdate) {
                deleteMaterial();
                materialNeedsUpdate = true;
            }
            _parentID = entity->getParentID();
        }
        if (entity->getMaterialMappingPos() != _materialMappingPos ||
            entity->getMaterialMappingScale() != _materialMappingScale ||
            entity->getMaterialMappingRot() != _materialMappingRot) {
            if (!materialNeedsUpdate) {
                deleteMaterial();
                materialNeedsUpdate = true;
            }
            _materialMappingPos = entity->getMaterialMappingPos();
            _materialMappingScale = entity->getMaterialMappingScale();
            _materialMappingRot = entity->getMaterialMappingRot();
        }
        if (entity->getPriority() != _priority) {
            if (!materialNeedsUpdate) {
                deleteMaterial();
                materialNeedsUpdate = true;
            }
            _priority = entity->getPriority();
        }
        if (entity->getParentMaterialName() != _parentMaterialName) {
            if (!materialNeedsUpdate) {
                deleteMaterial();
                materialNeedsUpdate = true;
            }
            _parentMaterialName = entity->getParentMaterialName();
        }
        if (entity->getMaterialData() != _materialData) {
            if (!materialNeedsUpdate) {
                deleteMaterial();
            }
            _materialData = entity->getMaterialData();
            if (_materialURL.startsWith("materialData")) {
                // Trigger material update when material data changes
                materialNeedsUpdate = true;
                setMaterialURL(_materialURL, true);
            }
        }

        if (_retryApply || materialNeedsUpdate) {
            applyMaterial();
            if (_retryApply) {
                emit requestRenderUpdate();
            }
        }

        _renderTransform = getModelTransform();
        const float MATERIAL_ENTITY_SCALE = 0.5f;
        _renderTransform.postScale(MATERIAL_ENTITY_SCALE);
        _renderTransform.postScale(ENTITY_ITEM_DEFAULT_DIMENSIONS);
    });
}

ItemKey MaterialEntityRenderer::getKey() {
    ItemKey::Builder builder;
    builder.withTypeShape().withTagBits(getTagMask());

    if (!_visible) {
        builder.withInvisible();
    }

    const auto& drawMaterial = getMaterial();
    if (drawMaterial) {
        auto matKey = drawMaterial->getKey();
        if (matKey.isTranslucent()) {
            builder.withTransparent();
        }
    }

    return builder.build();
}

ShapeKey MaterialEntityRenderer::getShapeKey() {
    ShapeKey::Builder builder;
    graphics::MaterialKey drawMaterialKey;
    const auto& drawMaterial = getMaterial();
    if (drawMaterial) {
        drawMaterialKey = drawMaterial->getKey();
    }

    if (drawMaterialKey.isTranslucent()) {
        builder.withTranslucent();
    }
    if (drawMaterial && drawMaterial->getProcedural().isReady()) {
        builder.withOwnPipeline();
    } else {
        bool hasTangents = drawMaterialKey.isNormalMap();
        bool hasLightmap = drawMaterialKey.isLightmapMap();
        bool isUnlit = drawMaterialKey.isUnlit();

        builder.withMaterial();

        if (hasTangents) {
            builder.withTangents();
        }
        if (hasLightmap) {
            builder.withLightmap();
        }
        if (isUnlit) {
            builder.withUnlit();
        }
    }

    return builder.build();
}

void MaterialEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderableMaterialEntityItem::render");
    Q_ASSERT(args->_batch);
    gpu::Batch& batch = *args->_batch;

    // Don't render if our parent is set or our material is null
    QUuid parentID;
    Transform renderTransform;
    graphics::ProceduralMaterialPointer drawMaterial;
    bool proceduralRender = false;
    glm::vec4 outColor;
    Transform textureTransform;
    withReadLock([&] {
        parentID = _parentID;
        renderTransform = _renderTransform;
        drawMaterial = getMaterial();
        textureTransform.setTranslation(glm::vec3(_materialMappingPos, 0));
        textureTransform.setRotation(glm::vec3(0, 0, glm::radians(_materialMappingRot)));
        textureTransform.setScale(glm::vec3(_materialMappingScale, 1));
        if (drawMaterial && drawMaterial->getProcedural().isReady()) {
            outColor = glm::vec4(drawMaterial->getAlbedo(), 1.0f);
            outColor = drawMaterial->getProcedural().getColor(outColor);
            drawMaterial->editProcedural().prepare(batch, renderTransform.getTranslation(), renderTransform.getScale(), renderTransform.getRotation(), ProceduralProgramKey());
            proceduralRender = true;
        }
    });
    if (!parentID.isNull() || !drawMaterial) {
        return;
    }

    batch.setModelTransform(renderTransform);

    if (!proceduralRender) {
        if (args->_renderMode != render::Args::RenderMode::SHADOW_RENDER_MODE) {
            drawMaterial->setTextureTransforms(textureTransform);

            // bind the material
            RenderPipelines::bindMaterial(drawMaterial, batch, args->_enableTexturing);
            args->_details._materialSwitches++;
        }

        // Draw!
        DependencyManager::get<GeometryCache>()->renderSphere(batch);
    } else {
        DependencyManager::get<GeometryCache>()->renderSphere(batch, outColor);
    }

    args->_details._trianglesRendered += (int)DependencyManager::get<GeometryCache>()->getSphereTriangleCount();
}

void MaterialEntityRenderer::setMaterialURL(const QString& materialURLString, bool materialDataChanged) {
    bool usingMaterialData = materialDataChanged || materialURLString.startsWith("materialData");
    if (!materialDataChanged || usingMaterialData) {
        _materialURL = materialURLString;

        if (materialURLString.contains("?")) {
            auto split = materialURLString.split("?");
            _currentMaterialName = split.last().toStdString();
        }

        if (usingMaterialData) {
            _parsedMaterials = ProceduralMaterialResource::parseJSONMaterials(QJsonDocument::fromJson(_materialData.toUtf8()), materialURLString);

            // Since our material changed, the current name might not be valid anymore, so we need to update
            setCurrentMaterialName(_currentMaterialName);
        } else {
            _networkMaterial = MaterialCache::instance().getMaterial(materialURLString);
            auto onMaterialRequestFinished = [&](bool success) {
                if (success) {
                    _parsedMaterials = _networkMaterial->parsedMaterials;

                    setCurrentMaterialName(_currentMaterialName);
                    applyMaterial();
                }
            };
            if (_networkMaterial) {
                if (_networkMaterial->isLoaded()) {
                    onMaterialRequestFinished(!_networkMaterial->isFailed());
                } else {
                    connect(_networkMaterial.data(), &Resource::finished, this, onMaterialRequestFinished);
                }
            }
        }
    }
}

void MaterialEntityRenderer::setCurrentMaterialName(const std::string& currentMaterialName) {
    if (_parsedMaterials.networkMaterials.find(currentMaterialName) != _parsedMaterials.networkMaterials.end()) {
        _currentMaterialName = currentMaterialName;
    } else if (_parsedMaterials.names.size() > 0) {
        _currentMaterialName = _parsedMaterials.names[0];
    }
}

graphics::ProceduralMaterialPointer MaterialEntityRenderer::getMaterial() const {
    auto material = _parsedMaterials.networkMaterials.find(_currentMaterialName);
    if (material != _parsedMaterials.networkMaterials.end()) {
        return material->second;
    } else {
        return nullptr;
    }
}

void MaterialEntityRenderer::deleteMaterial() {
    graphics::ProceduralMaterialPointer material = getMaterial();
    if (!material) {
        return;
    }
    QUuid parentID = _parentID;
    if (parentID.isNull()) {
        return;
    }

    // Our parent could be an entity, an avatar, or an overlay
    if (EntityTreeRenderer::removeMaterialFromEntity(parentID, material, _parentMaterialName.toStdString())) {
        return;
    }

    if (EntityTreeRenderer::removeMaterialFromAvatar(parentID, material, _parentMaterialName.toStdString())) {
        return;
    }

    if (EntityTreeRenderer::removeMaterialFromOverlay(parentID, material, _parentMaterialName.toStdString())) {
        return;
    }

    // if a remove fails, our parent is gone, so we don't need to retry
}

void MaterialEntityRenderer::applyMaterial() {
    _retryApply = false;
    graphics::ProceduralMaterialPointer material = getMaterial();
    QUuid parentID = _parentID;
    if (!material || parentID.isNull()) {
        return;
    }
    Transform textureTransform;
    textureTransform.setTranslation(glm::vec3(_materialMappingPos, 0));
    textureTransform.setRotation(glm::vec3(0, 0, glm::radians(_materialMappingRot)));
    textureTransform.setScale(glm::vec3(_materialMappingScale, 1));
    material->setTextureTransforms(textureTransform);

    graphics::MaterialLayer materialLayer = graphics::MaterialLayer(material, _priority);

    // Our parent could be an entity, an avatar, or an overlay
    if (EntityTreeRenderer::addMaterialToEntity(parentID, materialLayer, _parentMaterialName.toStdString())) {
        return;
    }

    if (EntityTreeRenderer::addMaterialToAvatar(parentID, materialLayer, _parentMaterialName.toStdString())) {
        return;
    }

    if (EntityTreeRenderer::addMaterialToOverlay(parentID, materialLayer, _parentMaterialName.toStdString())) {
        return;
    }

    // if we've reached this point, we couldn't find our parent, so we need to try again later
    _retryApply = true;
}
//
//  Created by Sam Gondelman on 2/9/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "ProceduralMaterialCache.h"

#include "QJsonObject"
#include "QJsonDocument"
#include "QJsonArray"

#include "RegisteredMetaTypes.h"

ProceduralMaterialResource::ProceduralMaterialResource(const QUrl& url) :
    Resource(url) {}

void ProceduralMaterialResource::downloadFinished(const QByteArray& data) {
    parsedMaterials.reset();

    if (_url.toString().contains(".json")) {
        parsedMaterials = parseJSONMaterials(QJsonDocument::fromJson(data), _url);
    }

    // TODO: parse other material types

    finishedLoading(true);
}

/**jsdoc
 * <p>An RGB or SRGB color value.</p>
 * <table>
 *   <thead>
 *     <tr><th>Index</th><th>Type</th><th>Attributes</th><th>Default</th><th>Value</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>0</code></td><td>number</td><td></td><td></td>
 *       <td>Red component value. Number in the range <code>0.0</code> &ndash; <code>1.0</code>.</td></tr>
 *     <tr><td><code>1</code></td><td>number</td><td></td><td></td>
 *       <td>Green component value. Number in the range <code>0.0</code> &ndash; <code>1.0</code>.</td></tr>
 *     <tr><td><code>2</code></td><td>number</td><td></td><td></td>
 *       <td>Blue component value. Number in the range <code>0.0</code> &ndash; <code>1.0</code>.</td></tr>
 *     <tr><td><code>3</code></td><td>boolean</td><td>&lt;optional&gt;</td><td>false</td>
 *       <td>If <code>true</code> then the color is an SRGB color.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {array} RGBS
 */
bool ProceduralMaterialResource::parseJSONColor(const QJsonValue& array, glm::vec3& color, bool& isSRGB) {
    if (array.isArray()) {
        QJsonArray colorArray = array.toArray();
        if (colorArray.size() >= 3 && colorArray[0].isDouble() && colorArray[1].isDouble() && colorArray[2].isDouble()) {
            isSRGB = true;
            if (colorArray.size() >= 4) {
                if (colorArray[3].isBool()) {
                    isSRGB = colorArray[3].toBool();
                }
            }
            color = glm::vec3(colorArray[0].toDouble(), colorArray[1].toDouble(), colorArray[2].toDouble());
            return true;
        }
    } else if (array.isObject()) {
        bool toReturn;
        isSRGB = true;
        color = vec3FromVariant(array.toObject(), toReturn);
        return toReturn;
    }
    return false;
}

/**jsdoc
 * A material or set of materials such as may be used by a {@link Entities.EntityType|Material} entity.
 * @typedef {object} MaterialResource
 * @property {number} materialVersion=1 - The version of the material. <em>Currently not used.</em>
 * @property {Material|Material[]} materials - The details of the material or materials.
 */
ProceduralMaterialResource::ParsedMaterials ProceduralMaterialResource::parseJSONMaterials(const QJsonDocument& materialJSON,
                                                                                           const QUrl& baseUrl) {
    ParsedMaterials toReturn;
    if (!materialJSON.isNull() && materialJSON.isObject()) {
        QJsonObject materialJSONObject = materialJSON.object();
        for (auto& key : materialJSONObject.keys()) {
            if (key == "materialVersion") {
                auto value = materialJSONObject.value(key);
                if (value.isDouble()) {
                    toReturn.version = (uint)value.toInt();
                }
            } else if (key == "materials") {
                auto materialsValue = materialJSONObject.value(key);
                if (materialsValue.isArray()) {
                    QJsonArray materials = materialsValue.toArray();
                    for (auto material : materials) {
                        if (!material.isNull() && material.isObject()) {
                            auto parsedMaterial = parseJSONMaterial(material.toObject(), baseUrl);
                            toReturn.networkMaterials[parsedMaterial.first] = parsedMaterial.second;
                            toReturn.names.push_back(parsedMaterial.first);
                        }
                    }
                } else if (materialsValue.isObject()) {
                    auto parsedMaterial = parseJSONMaterial(materialsValue.toObject(), baseUrl);
                    toReturn.networkMaterials[parsedMaterial.first] = parsedMaterial.second;
                    toReturn.names.push_back(parsedMaterial.first);
                }
            }
        }
    }

    return toReturn;
}

/**jsdoc
 * A material such as may be used by a {@link Entities.EntityType|Material} entity.
 * @typedef {object} Material
 * @property {string} model="hifi_pbr" - Different material models support different properties and rendering modes.
 *     Supported models are: "hifi_pbr", "hifi_shader_simple"
 * @property {string} name="" - A name for the material.  Supported by all material models.
 * @property {Vec3Color|RGBS} albedo - The albedo color. A {@link Vec3Color} value is treated as sRGB. A {@link RGBS} value can 
 *     be either RGB or sRGB.  Supported by all material models.
 * @property {number} opacity=1.0 - The opacity, <code>0.0</code> &ndash; <code>1.0</code>.  Supported by all material models.
 * @property {Vec3Color|RGBS} emissive - The emissive color, i.e., the color that the material emits. A {@link Vec3Color} value
 *     is treated as sRGB. A {@link RGBS} value can be either RGB or sRGB. "hifi_pbr" model only.
 * @property {boolean} unlit=false - If <code>true</code>, the material is not lit. "hifi_pbr" model only.
 * @property {number} roughness - The roughness, <code>0.0</code> &ndash; <code>1.0</code>.  "hifi_pbr" model only.
 * @property {number} metallic - The metallicness, <code>0.0</code> &ndash; <code>1.0</code>.  "hifi_pbr" model only.
 * @property {number} scattering - The scattering, <code>0.0</code> &ndash; <code>1.0</code>.  "hifi_pbr" model only.
 * @property {string} emissiveMap - URL of emissive texture image.  "hifi_pbr" model only.
 * @property {string} albedoMap - URL of albedo texture image.  "hifi_pbr" model only.
 * @property {string} opacityMap - URL of opacity texture image. Set value the same as the <code>albedoMap</code> value for 
 *     transparency.  "hifi_pbr" model only.
 * @property {string} roughnessMap - URL of roughness texture image. Can use this or <code>glossMap</code>, but not both.  "hifi_pbr" model only.
 * @property {string} glossMap - URL of gloss texture image. Can use this or <code>roughnessMap</code>, but not both.  "hifi_pbr" model only.
 * @property {string} metallicMap - URL of metallic texture image. Can use this or <code>specularMap</code>, but not both.  "hifi_pbr" model only.
 * @property {string} specularMap - URL of specular texture image. Can use this or <code>metallicMap</code>, but not both.  "hifi_pbr" model only.
 * @property {string} normalMap - URL of normal texture image. Can use this or <code>bumpMap</code>, but not both.  "hifi_pbr" model only.
 * @property {string} bumpMap - URL of bump texture image. Can use this or <code>normalMap</code>, but not both.  "hifi_pbr" model only.
 * @property {string} occlusionMap - URL of occlusion texture image.  "hifi_pbr" model only.
 * @property {string} scatteringMap - URL of scattering texture image. Only used if <code>normalMap</code> or 
 *     <code>bumpMap</code> is specified.  "hifi_pbr" model only.
 * @property {string} lightMap - URL of light map texture image. <em>Currently not used.</em>  "hifi_pbr" model only.
 * @property {ProceduralData} procedural - The definition of a procedural shader material.  "hifi_shader_simple" model only.
 */
// Note: See MaterialEntityItem.h for default values used in practice.
std::pair<std::string, graphics::ProceduralMaterialPointer> ProceduralMaterialResource::parseJSONMaterial(const QJsonObject& materialJSON, const QUrl& baseUrl) {
    std::string name = "";
    graphics::ProceduralMaterialPointer material = std::make_shared<graphics::ProceduralMaterial>();
    std::string modelString = "hifi_pbr";
    auto modelJSON = materialJSON.value("model");
    if (modelJSON.isString()) {
        modelString = modelJSON.toString().toStdString();
        material->setModel(modelString);
    }
    if (modelString == "hifi_pbr") {
        for (auto& key : materialJSON.keys()) {
            if (key == "name") {
                auto nameJSON = materialJSON.value(key);
                if (nameJSON.isString()) {
                    name = nameJSON.toString().toStdString();
                }
            } else if (key == "emissive") {
                glm::vec3 color;
                bool isSRGB;
                bool valid = parseJSONColor(materialJSON.value(key), color, isSRGB);
                if (valid) {
                    material->setEmissive(color, isSRGB);
                }
            } else if (key == "opacity") {
                auto value = materialJSON.value(key);
                if (value.isDouble()) {
                    material->setOpacity(value.toDouble());
                }
            } else if (key == "unlit") {
                auto value = materialJSON.value(key);
                if (value.isBool()) {
                    material->setUnlit(value.toBool());
                }
            } else if (key == "albedo") {
                glm::vec3 color;
                bool isSRGB;
                bool valid = parseJSONColor(materialJSON.value(key), color, isSRGB);
                if (valid) {
                    material->setAlbedo(color, isSRGB);
                }
            } else if (key == "roughness") {
                auto value = materialJSON.value(key);
                if (value.isDouble()) {
                    material->setRoughness(value.toDouble());
                }
            } else if (key == "metallic") {
                auto value = materialJSON.value(key);
                if (value.isDouble()) {
                    material->setMetallic(value.toDouble());
                }
            } else if (key == "scattering") {
                auto value = materialJSON.value(key);
                if (value.isDouble()) {
                    material->setScattering(value.toDouble());
                }
            } else if (key == "emissiveMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setEmissiveMap(baseUrl.resolved(value.toString()));
                }
            } else if (key == "albedoMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    QString urlString = value.toString();
                    bool useAlphaChannel = false;
                    auto opacityMap = materialJSON.find("opacityMap");
                    if (opacityMap != materialJSON.end() && opacityMap->isString() && opacityMap->toString() == urlString) {
                        useAlphaChannel = true;
                    }
                    material->setAlbedoMap(baseUrl.resolved(urlString), useAlphaChannel);
                }
            } else if (key == "roughnessMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setRoughnessMap(baseUrl.resolved(value.toString()), false);
                }
            } else if (key == "glossMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setRoughnessMap(baseUrl.resolved(value.toString()), true);
                }
            } else if (key == "metallicMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setMetallicMap(baseUrl.resolved(value.toString()), false);
                }
            } else if (key == "specularMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setMetallicMap(baseUrl.resolved(value.toString()), true);
                }
            } else if (key == "normalMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setNormalMap(baseUrl.resolved(value.toString()), false);
                }
            } else if (key == "bumpMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setNormalMap(baseUrl.resolved(value.toString()), true);
                }
            } else if (key == "occlusionMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setOcclusionMap(baseUrl.resolved(value.toString()));
                }
            } else if (key == "scatteringMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setScatteringMap(baseUrl.resolved(value.toString()));
                }
            } else if (key == "lightMap") {
                auto value = materialJSON.value(key);
                if (value.isString()) {
                    material->setLightmapMap(baseUrl.resolved(value.toString()));
                }
            }
        }
    } else if (modelString == "hifi_shader_simple") {
        for (auto& key : materialJSON.keys()) {
            if (key == "name") {
                auto nameJSON = materialJSON.value(key);
                if (nameJSON.isString()) {
                    name = nameJSON.toString().toStdString();
                }
            } else if (key == "opacity") {
                auto value = materialJSON.value(key);
                if (value.isDouble()) {
                    material->setOpacity(value.toDouble());
                }
            } else if (key == "albedo") {
                glm::vec3 color;
                bool isSRGB;
                bool valid = parseJSONColor(materialJSON.value(key), color, isSRGB);
                if (valid) {
                    material->setAlbedo(color, isSRGB);
                }
            } else if (key == "procedural") {
                auto value = materialJSON.value(key);
                material->setProceduralData(QJsonDocument::fromVariant(value.toVariant()).toJson());
            }
        }
    }
    return std::pair<std::string, graphics::ProceduralMaterialPointer>(name, material);
}

MaterialCache& MaterialCache::instance() {
    static MaterialCache _instance;
    return _instance;
}

ProceduralMaterialResourcePointer MaterialCache::getMaterial(const QUrl& url) {
    return ResourceCache::getResource(url, QUrl(), nullptr).staticCast<ProceduralMaterialResource>();
}

QSharedPointer<Resource> MaterialCache::createResource(const QUrl& url, const QSharedPointer<Resource>& fallback, const void* extra) {
    return QSharedPointer<Resource>(new ProceduralMaterialResource(url), &Resource::deleter);
}
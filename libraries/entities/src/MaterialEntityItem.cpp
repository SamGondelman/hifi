//
//  Created by Sam Gondelman on 1/12/18
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "MaterialEntityItem.h"

#include "EntityItemProperties.h"

#include "QJsonDocument"
#include "QJsonArray"

EntityItemPointer MaterialEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    Pointer entity(new MaterialEntityItem(entityID), [](EntityItem* ptr) { ptr->deleteLater(); });
    entity->setProperties(properties);
    return entity;
}

// our non-pure virtual subclass for now...
MaterialEntityItem::MaterialEntityItem(const EntityItemID& entityItemID) : EntityItem(entityItemID) {
    _type = EntityTypes::Material;
}

EntityItemProperties MaterialEntityItem::getProperties(EntityPropertyFlags desiredProperties) const {
    EntityItemProperties properties = EntityItem::getProperties(desiredProperties); // get the properties from our base class
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(materialURL, getMaterialURL);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(materialMappingMode, getMaterialMappingMode);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(priority, getPriority);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(parentMaterialName, getParentMaterialName);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(materialMappingPos, getMaterialMappingPos);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(materialMappingScale, getMaterialMappingScale);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(materialMappingRot, getMaterialMappingRot);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(materialData, getMaterialData);
    return properties;
}

bool MaterialEntityItem::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = EntityItem::setProperties(properties); // set the properties in our base class

    SET_ENTITY_PROPERTY_FROM_PROPERTIES(materialURL, setMaterialURL);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(materialMappingMode, setMaterialMappingMode);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(priority, setPriority);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(parentMaterialName, setParentMaterialName);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(materialMappingPos, setMaterialMappingPos);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(materialMappingScale, setMaterialMappingScale);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(materialMappingRot, setMaterialMappingRot);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(materialData, setMaterialData);

    if (somethingChanged) {
        bool wantDebug = false;
        if (wantDebug) {
            uint64_t now = usecTimestampNow();
            int elapsed = now - getLastEdited();
            qCDebug(entities) << "MaterialEntityItem::setProperties() AFTER update... edited AGO=" << elapsed <<
                    "now=" << now << " getLastEdited()=" << getLastEdited();
        }
        setLastEdited(properties.getLastEdited());
    }
    return somethingChanged;
}

int MaterialEntityItem::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_MATERIAL_URL, QString, setMaterialURL);
    READ_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_MODE, MaterialMappingMode, setMaterialMappingMode);
    READ_ENTITY_PROPERTY(PROP_MATERIAL_PRIORITY, quint16, setPriority);
    READ_ENTITY_PROPERTY(PROP_PARENT_MATERIAL_NAME, QString, setParentMaterialName);
    READ_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_POS, glm::vec2, setMaterialMappingPos);
    READ_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_SCALE, glm::vec2, setMaterialMappingScale);
    READ_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_ROT, float, setMaterialMappingRot);
    READ_ENTITY_PROPERTY(PROP_MATERIAL_DATA, QString, setMaterialData);

    return bytesRead;
}

EntityPropertyFlags MaterialEntityItem::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties = EntityItem::getEntityProperties(params);
    requestedProperties += PROP_MATERIAL_URL;
    requestedProperties += PROP_MATERIAL_MAPPING_MODE;
    requestedProperties += PROP_MATERIAL_PRIORITY;
    requestedProperties += PROP_PARENT_MATERIAL_NAME;
    requestedProperties += PROP_MATERIAL_MAPPING_POS;
    requestedProperties += PROP_MATERIAL_MAPPING_SCALE;
    requestedProperties += PROP_MATERIAL_MAPPING_ROT;
    requestedProperties += PROP_MATERIAL_DATA;
    return requestedProperties;
}

void MaterialEntityItem::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                    EntityTreeElementExtraEncodeDataPointer modelTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount, 
                                    OctreeElement::AppendState& appendState) const { 

    bool successPropertyFits = true;
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_URL, getMaterialURL());
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_MODE, (uint32_t)getMaterialMappingMode());
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_PRIORITY, getPriority());
    APPEND_ENTITY_PROPERTY(PROP_PARENT_MATERIAL_NAME, getParentMaterialName());
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_POS, getMaterialMappingPos());
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_SCALE, getMaterialMappingScale());
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_MAPPING_ROT, getMaterialMappingRot());
    APPEND_ENTITY_PROPERTY(PROP_MATERIAL_DATA, getMaterialData());
}

void MaterialEntityItem::debugDump() const {
    quint64 now = usecTimestampNow();
    qCDebug(entities) << " MATERIAL EntityItem id:" << getEntityItemID() << "---------------------------------------------";
    qCDebug(entities) << "                   name:" << _name;
    qCDebug(entities) << "           material url:" << _materialURL;
    qCDebug(entities) << "  material mapping mode:" << _materialMappingMode;
    qCDebug(entities) << "               priority:" << _priority;
    qCDebug(entities) << "   parent material name:" << _parentMaterialName;
    qCDebug(entities) << "   material mapping pos:" << _materialMappingPos;
    qCDebug(entities) << " material mapping scale:" << _materialMappingRot;
    qCDebug(entities) << "   material mapping rot:" << _materialMappingScale;
    qCDebug(entities) << "               position:" << debugTreeVector(getWorldPosition());
    qCDebug(entities) << "             dimensions:" << debugTreeVector(getScaledDimensions());
    qCDebug(entities) << "          getLastEdited:" << debugTime(getLastEdited(), now);
    qCDebug(entities) << "MATERIAL EntityItem Ptr:" << this;
}

void MaterialEntityItem::setUnscaledDimensions(const glm::vec3& value) {
    EntityItem::setUnscaledDimensions(ENTITY_ITEM_DEFAULT_DIMENSIONS);
}

void MaterialEntityItem::setMaterialURL(const QString& materialURL) {
    if (_materialURL != materialURL) {
        _materialURL = materialURL;
    }
}

void MaterialEntityItem::setMaterialData(const QString& materialData) {
    if (_materialData != materialData) {
        _materialData = materialData;
    }
}

void MaterialEntityItem::setMaterialMappingPos(const glm::vec2& materialMappingPos) {
    if (_materialMappingPos != materialMappingPos) {
        _materialMappingPos = materialMappingPos;
    }
}

void MaterialEntityItem::setMaterialMappingScale(const glm::vec2& materialMappingScale) {
    if (_materialMappingScale != materialMappingScale) {
        _materialMappingScale = materialMappingScale;
    }
}

void MaterialEntityItem::setMaterialMappingRot(const float& materialMappingRot) {
    if (_materialMappingRot != materialMappingRot) {
        _materialMappingRot = materialMappingRot;
    }
}

void MaterialEntityItem::setPriority(quint16 priority) {
    if (_priority != priority) {
        _priority = priority;
    }
}

void MaterialEntityItem::setParentMaterialName(const QString& parentMaterialName) {
    if (_parentMaterialName != parentMaterialName) {
        _parentMaterialName = parentMaterialName;
    }
}
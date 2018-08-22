//
//  Created by Sam Gondelman on 1/18/2018
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderableMaterialEntityItem_h
#define hifi_RenderableMaterialEntityItem_h

#include "RenderableEntityItem.h"

#include <MaterialEntityItem.h>

#include <procedural/ProceduralMaterial.h>
#include <procedural/ProceduralMaterialCache.h>

namespace render { namespace entities { 

class MaterialEntityRenderer : public TypedEntityRenderer<MaterialEntityItem> {
    using Parent = TypedEntityRenderer<MaterialEntityItem>;
    using Pointer = std::shared_ptr<MaterialEntityRenderer>;
public:
    MaterialEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {}
    ~MaterialEntityRenderer() { removeMaterial(); }

private:
    virtual bool needsRenderUpdate() const override;
    virtual bool needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const override;
    virtual void doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) override;
    virtual void doRender(RenderArgs* args) override;

    ItemKey getKey() override;
    ShapeKey getShapeKey() override;

    QString _materialURL;
    QUuid _parentID;
    glm::vec2 _materialMappingPos;
    glm::vec2 _materialMappingScale;
    float _materialMappingRot;
    quint16 _priority;
    QString _parentMaterialName;
    QString _materialData;

    graphics::ProceduralMaterialPointer getMaterial() const;
    void setMaterialURL(const QString& materialURLString, bool materialDataChanged = false);
    void setCurrentMaterialName(const std::string& currentMaterialName);

    void applyMaterial();
    void removeMaterial();

    ProceduralMaterialResourcePointer _networkMaterial;
    ProceduralMaterialResource::ParsedMaterials _parsedMaterials;
    std::string _currentMaterialName;

    bool _retryApply { false };
};

} } 
#endif // hifi_RenderableMaterialEntityItem_h

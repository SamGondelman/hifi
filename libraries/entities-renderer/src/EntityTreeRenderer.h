//
//  EntityTreeRenderer.h
//  interface/src
//
//  Created by Brad Hefta-Gaub on 12/6/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityTreeRenderer_h
#define hifi_EntityTreeRenderer_h

#include <QtCore/QSet>
#include <QtCore/QStack>
#include <QtGui/QMouseEvent>

#include <AbstractAudioInterface.h>
#include <EntityScriptingInterface.h> // for RayToEntityIntersectionResult
#include <EntityTree.h>
#include <PointerEvent.h>
#include <ScriptCache.h>
#include <TextureCache.h>
#include <OctreeProcessor.h>
#include <render/Forward.h>
#include <workload/Space.h>

#include "procedural/ProceduralMaterial.h"

class AbstractScriptingServicesInterface;
class AbstractViewStateInterface;
class Model;
class ScriptEngine;
class ZoneEntityItem;
class EntityItem;

namespace render { namespace entities {
    class EntityRenderer;
    using EntityRendererPointer = std::shared_ptr<EntityRenderer>;
    using EntityRendererWeakPointer = std::weak_ptr<EntityRenderer>;

} }

using EntityRenderer = render::entities::EntityRenderer;
using EntityRendererPointer = render::entities::EntityRendererPointer;
using EntityRendererWeakPointer = render::entities::EntityRendererWeakPointer;
class Model;
using ModelPointer = std::shared_ptr<Model>;
using ModelWeakPointer = std::weak_ptr<Model>;

using CalculateEntityLoadingPriority = std::function<float(const EntityItem& item)>;

// Generic client side Octree renderer class.
class EntityTreeRenderer : public OctreeProcessor, public Dependency {
    Q_OBJECT
public:
    static void setEntitiesShouldFadeFunction(std::function<bool()> func) { _entitiesShouldFadeFunction = func; }
    static std::function<bool()> getEntitiesShouldFadeFunction() { return _entitiesShouldFadeFunction; }

    EntityTreeRenderer(bool wantScripts, AbstractViewStateInterface* viewState,
                                AbstractScriptingServicesInterface* scriptingServices);
    virtual ~EntityTreeRenderer();

    QSharedPointer<EntityTreeRenderer> getSharedFromThis() {
        return qSharedPointerCast<EntityTreeRenderer>(sharedFromThis());
    }

    virtual char getMyNodeType() const override { return NodeType::EntityServer; }
    virtual PacketType getMyQueryMessageType() const override { return PacketType::EntityQuery; }
    virtual PacketType getExpectedPacketType() const override { return PacketType::EntityData; }

    // Returns the priority at which an entity should be loaded. Higher values indicate higher priority.
    static float getEntityLoadingPriority(const EntityItem& item) { return _calculateEntityLoadingPriorityFunc(item); }
    static void setEntityLoadingPriorityFunction(CalculateEntityLoadingPriority fn) { _calculateEntityLoadingPriorityFunc = fn; }

    void setMouseRayPickID(unsigned int rayPickID) { _mouseRayPickID = rayPickID; }
    void setMouseRayPickResultOperator(std::function<RayToEntityIntersectionResult(unsigned int)> getPrevRayPickResultOperator) { _getPrevRayPickResultOperator = getPrevRayPickResultOperator;  }
    void setSetPrecisionPickingOperator(std::function<void(unsigned int, bool)> setPrecisionPickingOperator) { _setPrecisionPickingOperator = setPrecisionPickingOperator; }

    void shutdown();
    void update(bool simulate);

    EntityTreePointer getTree() { return std::static_pointer_cast<EntityTree>(_tree); }

    void processEraseMessage(ReceivedMessage& message, const SharedNodePointer& sourceNode);

    virtual void init() override;

    /// clears the tree
    virtual void clear() override;

    /// reloads the entity scripts, calling unload and preload
    void reloadEntityScripts();

    // event handles which may generate entity related events
    void mouseReleaseEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseDoublePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);

    /// connect our signals to anEntityScriptingInterface for firing of events related clicking,
    /// hovering over, and entering entities
    void connectSignalsToSlots(EntityScriptingInterface* entityScriptingInterface);

    // For Scene.shouldRenderEntities
    QList<EntityItemID>& getEntitiesLastInScene() { return _entityIDsLastInScene; }

    std::shared_ptr<ZoneEntityItem> myAvatarZone() { return _layeredZones.getZone(); }

    bool wantsKeyboardFocus(const EntityItemID& id) const;
    QObject* getEventHandler(const EntityItemID& id);
    bool wantsHandControllerPointerEvents(const EntityItemID& id) const;
    void setProxyWindow(const EntityItemID& id, QWindow* proxyWindow);
    void setCollisionSound(const EntityItemID& id, const SharedSoundPointer& sound);
    EntityItemPointer getEntity(const EntityItemID& id);
    void onEntityChanged(const EntityItemID& id);

    // Access the workload Space
    workload::SpacePointer getWorkloadSpace() const { return _space; }

    static void setAddMaterialToEntityOperator(std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> addMaterialToEntityOperator) { _addMaterialToEntityOperator = addMaterialToEntityOperator; }
    static void setRemoveMaterialFromEntityOperator(std::function<bool(const QUuid&, graphics::ProceduralMaterialPointer, const std::string&)> removeMaterialFromEntityOperator) { _removeMaterialFromEntityOperator = removeMaterialFromEntityOperator; }
    static bool addMaterialToEntity(const QUuid& entityID, graphics::MaterialLayer material, const std::string& parentMaterialName);
    static bool removeMaterialFromEntity(const QUuid& entityID, graphics::ProceduralMaterialPointer material, const std::string& parentMaterialName);

    static void setAddMaterialToAvatarOperator(std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> addMaterialToAvatarOperator) { _addMaterialToAvatarOperator = addMaterialToAvatarOperator; }
    static void setRemoveMaterialFromAvatarOperator(std::function<bool(const QUuid&, graphics::ProceduralMaterialPointer, const std::string&)> removeMaterialFromAvatarOperator) { _removeMaterialFromAvatarOperator = removeMaterialFromAvatarOperator; }
    static bool addMaterialToAvatar(const QUuid& avatarID, graphics::MaterialLayer material, const std::string& parentMaterialName);
    static bool removeMaterialFromAvatar(const QUuid& avatarID, graphics::ProceduralMaterialPointer material, const std::string& parentMaterialName);

    static void setAddMaterialToOverlayOperator(std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> addMaterialToOverlayOperator) { _addMaterialToOverlayOperator = addMaterialToOverlayOperator; }
    static void setRemoveMaterialFromOverlayOperator(std::function<bool(const QUuid&, graphics::ProceduralMaterialPointer, const std::string&)> removeMaterialFromOverlayOperator) { _removeMaterialFromOverlayOperator = removeMaterialFromOverlayOperator; }
    static bool addMaterialToOverlay(const QUuid& overlayID, graphics::MaterialLayer material, const std::string& parentMaterialName);
    static bool removeMaterialFromOverlay(const QUuid& overlayID, graphics::ProceduralMaterialPointer material, const std::string& parentMaterialName);

signals:
    void enterEntity(const EntityItemID& entityItemID);
    void leaveEntity(const EntityItemID& entityItemID);
    void collisionWithEntity(const EntityItemID& idA, const EntityItemID& idB, const Collision& collision);

public slots:
    void addingEntity(const EntityItemID& entityID);
    void deletingEntity(const EntityItemID& entityID);
    void entityScriptChanging(const EntityItemID& entityID, const bool reload);
    void entityCollisionWithEntity(const EntityItemID& idA, const EntityItemID& idB, const Collision& collision);
    void updateEntityRenderStatus(bool shouldRenderEntities);
    void updateZone(const EntityItemID& id);

    // optional slots that can be wired to menu items
    void setDisplayModelBounds(bool value) { _displayModelBounds = value; }
    void setPrecisionPicking(bool value) { _setPrecisionPickingOperator(_mouseRayPickID, value); }
    EntityRendererPointer renderableForEntityId(const EntityItemID& id) const;
    render::ItemID renderableIdForEntityId(const EntityItemID& id) const;

    void handleSpaceUpdate(std::pair<int32_t, glm::vec4> proxyUpdate);

protected:
    virtual OctreePointer createTree() override {
        EntityTreePointer newTree = EntityTreePointer(new EntityTree(true));
        newTree->createRootElement();
        return newTree;
    }

private:
    void addPendingEntities(const render::ScenePointer& scene, render::Transaction& transaction);
    void updateChangedEntities(const render::ScenePointer& scene, render::Transaction& transaction);
    EntityRendererPointer renderableForEntity(const EntityItemPointer& entity) const { return renderableForEntityId(entity->getID()); }
    render::ItemID renderableIdForEntity(const EntityItemPointer& entity) const { return renderableIdForEntityId(entity->getID()); }

    void resetEntitiesScriptEngine();

    bool findBestZoneAndMaybeContainingEntities(QVector<EntityItemID>* entitiesContainingAvatar = nullptr);

    bool applyLayeredZones();

    void checkAndCallPreload(const EntityItemID& entityID, bool reload = false, bool unloadFirst = false);

    EntityItemID _currentHoverOverEntityID;
    EntityItemID _currentClickingOnEntityID;

    QScriptValueList createEntityArgs(const EntityItemID& entityID);
    bool checkEnterLeaveEntities();
    void leaveAllEntities();
    void forceRecheckEntities();

    glm::vec3 _avatarPosition { 0.0f };
    QVector<EntityItemID> _currentEntitiesInside;

    bool _wantScripts;
    ScriptEnginePointer _entitiesScriptEngine;

    void playEntityCollisionSound(const EntityItemPointer& entity, const Collision& collision);

    bool _lastPointerEventValid;
    PointerEvent _lastPointerEvent;
    AbstractViewStateInterface* _viewState;
    AbstractScriptingServicesInterface* _scriptingServices;
    bool _displayModelBounds;

    bool _shuttingDown { false };

    QMultiMap<QUrl, EntityItemID> _waitingOnPreload;

    unsigned int _mouseRayPickID;
    std::function<RayToEntityIntersectionResult(unsigned int)> _getPrevRayPickResultOperator;
    std::function<void(unsigned int, bool)> _setPrecisionPickingOperator;

    static std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> _addMaterialToEntityOperator;
    static std::function<bool(const QUuid&, graphics::ProceduralMaterialPointer, const std::string&)> _removeMaterialFromEntityOperator;
    static std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> _addMaterialToAvatarOperator;
    static std::function<bool(const QUuid&, graphics::ProceduralMaterialPointer, const std::string&)> _removeMaterialFromAvatarOperator;
    static std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> _addMaterialToOverlayOperator;
    static std::function<bool(const QUuid&, graphics::ProceduralMaterialPointer, const std::string&)> _removeMaterialFromOverlayOperator;

    class LayeredZone {
    public:
        LayeredZone(std::shared_ptr<ZoneEntityItem> zone, QUuid id, float volume) : zone(zone), id(id), volume(volume) {}
        LayeredZone(std::shared_ptr<ZoneEntityItem> zone) : LayeredZone(zone, zone->getID(), zone->getVolumeEstimate()) {}

        bool operator<(const LayeredZone& r) const { return std::tie(volume, id) < std::tie(r.volume, r.id); }
        bool operator==(const LayeredZone& r) const { return id == r.id; }
        bool operator<=(const LayeredZone& r) const { return (*this < r) || (*this == r); }

        std::shared_ptr<ZoneEntityItem> zone;
        QUuid id;
        float volume;
    };

    class LayeredZones : public std::set<LayeredZone> {
    public:
        LayeredZones(EntityTreeRenderer* parent) : _entityTreeRenderer(parent) {}
        LayeredZones(LayeredZones&& other);

        // avoid accidental misconstruction
        LayeredZones() = delete;
        LayeredZones(const LayeredZones&) = delete;
        LayeredZones& operator=(const LayeredZones&) = delete;
        LayeredZones& operator=(LayeredZones&&) = delete;

        void clear();
        std::pair<iterator, bool> insert(const LayeredZone& layer);

        void apply();
        void update(std::shared_ptr<ZoneEntityItem> zone);

        bool contains(const LayeredZones& other);

        std::shared_ptr<ZoneEntityItem> getZone() { return empty() ? nullptr : begin()->zone; }

    private:
        void applyPartial(iterator layer);

        std::map<QUuid, iterator> _map;
        iterator _skyboxLayer{ end() };
        EntityTreeRenderer* _entityTreeRenderer;
    };

    LayeredZones _layeredZones;
    QString _zoneUserData;
    NetworkTexturePointer _ambientTexture;
    NetworkTexturePointer _skyboxTexture;
    QString _ambientTextureURL;
    QString _skyboxTextureURL;
    float _avgRenderableUpdateCost { 0.0f };
    bool _pendingAmbientTexture { false };
    bool _pendingSkyboxTexture { false };

    uint64_t _lastZoneCheck { 0 };
    const uint64_t ZONE_CHECK_INTERVAL = USECS_PER_MSEC * 100; // ~10hz
    const float ZONE_CHECK_DISTANCE = 0.001f;

    ReadWriteLockable _changedEntitiesGuard;
    std::unordered_set<EntityItemID> _changedEntities;

    std::unordered_map<EntityItemID, EntityRendererPointer> _renderablesToUpdate;
    std::unordered_map<EntityItemID, EntityRendererPointer> _entitiesInScene;
    std::unordered_map<EntityItemID, EntityItemWeakPointer> _entitiesToAdd;
    // For Scene.shouldRenderEntities
    QList<EntityItemID> _entityIDsLastInScene;

    static int _entitiesScriptEngineCount;
    static CalculateEntityLoadingPriority _calculateEntityLoadingPriorityFunc;
    static std::function<bool()> _entitiesShouldFadeFunction;

    mutable std::mutex _spaceLock;
    workload::SpacePointer _space{ new workload::Space() };
    workload::Transaction::Updates _spaceUpdates;
};


#endif // hifi_EntityTreeRenderer_h

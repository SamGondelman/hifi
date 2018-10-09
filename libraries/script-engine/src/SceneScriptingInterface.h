//
//  SceneScriptingInterface.h
//  libraries/script-engine
//
//  Created by Sam Gateau on 2/24/15.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_SceneScriptingInterface_h
#define hifi_SceneScriptingInterface_h

#include <qscriptengine.h> // QObject
#include <DependencyManager.h> // Dependency

/**jsdoc
 * @namespace Scene
 *
 * @hifi-interface
 * @hifi-client-entity
 *
 * @property {boolean} shouldRenderAvatars
 * @property {boolean} shouldRenderEntities
 */
class SceneScriptingInterface : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY

public:
    Q_PROPERTY(bool shouldRenderAvatars READ shouldRenderAvatars WRITE setShouldRenderAvatars)
    Q_PROPERTY(bool shouldRenderEntities READ shouldRenderEntities WRITE setShouldRenderEntities)
    bool shouldRenderAvatars() const { return _shouldRenderAvatars; }
    bool shouldRenderEntities() const { return _shouldRenderEntities; }
    void setShouldRenderAvatars(bool shouldRenderAvatars);
    void setShouldRenderEntities(bool shouldRenderEntities);

signals:

    /**jsdoc
     * @function Scene.shouldRenderAvatarsChanged
     * @param {boolean} shouldRenderAvatars
     * @returns {Signal}
     */
    void shouldRenderAvatarsChanged(bool shouldRenderAvatars);

    /**jsdoc
     * @function Scene.shouldRenderEntitiesChanged
     * @param {boolean} shouldRenderEntities
     * @returns {Signal}
     */
    void shouldRenderEntitiesChanged(bool shouldRenderEntities);

protected:
    SceneScriptingInterface() {}
    ~SceneScriptingInterface() {}

    bool _shouldRenderAvatars = true;
    bool _shouldRenderEntities = true;
};

#endif // hifi_SceneScriptingInterface_h 

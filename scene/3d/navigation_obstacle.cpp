/*************************************************************************/
/*  navigation_obstacle.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "navigation_obstacle.h"

#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "scene/3d/collision_shape_3d.h"
#include "scene/3d/navigation_3d.h"
#include "scene/3d/physics_body_3d.h"
#include "core/translation_helpers.h"
#include "servers/navigation_server.h"

IMPL_GDCLASS(NavigationObstacle)

void NavigationObstacle::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_navigation", {"navigation"}),&NavigationObstacle::set_navigation_node);
    MethodBinder::bind_method(D_METHOD("get_navigation"), &NavigationObstacle::get_navigation_node);
}

void NavigationObstacle::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {

            update_agent_shape();

            // Search the navigation node and set it
            {
                Navigation3D *nav = nullptr;
                Node *p = get_parent();
                while (p != nullptr) {
                    nav = object_cast<Navigation3D>(p);
                    if (nav != nullptr)
                        p = nullptr;
                    else
                        p = p->get_parent();
                }

                set_navigation(nav);
            }

            set_physics_process_internal(true);
        } break;
        case NOTIFICATION_EXIT_TREE: {
            set_navigation(nullptr);
            set_physics_process_internal(false);
        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
            Node3D *spatial = object_cast<Node3D>(get_parent());
            if (spatial) {
                NavigationServer::get_singleton()->agent_set_position(agent, spatial->get_global_transform().origin);
            }

            PhysicsBody3D *rigid = object_cast<PhysicsBody3D>(get_parent());
            if (rigid) {

                Vector3 v = rigid->get_linear_velocity();
                NavigationServer::get_singleton()->agent_set_velocity(agent, v);
                NavigationServer::get_singleton()->agent_set_target_velocity(agent, v);
            }

        } break;
    }
}

NavigationObstacle::NavigationObstacle() :
        navigation(nullptr),
        agent(RID()) {
    agent = NavigationServer::get_singleton()->agent_create();
}

NavigationObstacle::~NavigationObstacle() {
    NavigationServer::get_singleton()->free(agent);
    agent = RID(); // Pointless
}

void NavigationObstacle::set_navigation(Navigation3D *p_nav) {
    if (navigation == p_nav)
        return; // Pointless

    navigation = p_nav;
    NavigationServer::get_singleton()->agent_set_map(agent, navigation == nullptr ? RID() : navigation->get_rid());
}

void NavigationObstacle::set_navigation_node(Node *p_nav) {
    Navigation3D *nav = object_cast<Navigation3D>(p_nav);
    ERR_FAIL_COND(nav == nullptr);
    set_navigation(nav);
}

Node *NavigationObstacle::get_navigation_node() const {
    return object_cast<Node>(navigation);
}

StringName NavigationObstacle::get_configuration_warning() const {
    if (!object_cast<Node3D>(get_parent())) {

        return TTR("The NavigationObstacle only serves to provide collision avoidance to a spatial object.");
    }

    return StringName();
}

void NavigationObstacle::update_agent_shape() {
    Node *node = get_parent();

    // Estimate the radius of this physics body
    real_t radius = 0.0;
    for (int i(0); i < node->get_child_count(); i++) {
        // For each collision shape
        CollisionShape3D *cs = object_cast<CollisionShape3D>(node->get_child(i));
        if (cs) {
            // Take the distance between the Body center to the shape center
            real_t r = cs->get_transform().origin.length();
            if (cs->get_shape()) {
                // and add the enclosing shape radius
                r += cs->get_shape()->get_enclosing_radius();
            }
            Vector3 s = cs->get_global_transform().basis.get_scale();
            r *= M_MAX(s.x, M_MAX(s.y, s.z));
            // Takes the biggest radius
            radius = M_MAX(radius, r);
        }
    }
    Node3D *spa = object_cast<Node3D>(node);
    if (spa) {
        Vector3 s = spa->get_global_transform().basis.get_scale();
        radius *= M_MAX(s.x, M_MAX(s.y, s.z));
    }

    if (radius == 0.0f)
        radius = 1.0; // Never a 0 radius

    // Initialize the Agent as an object
    NavigationServer::get_singleton()->agent_set_neighbor_dist(agent, 0.0);
    NavigationServer::get_singleton()->agent_set_max_neighbors(agent, 0);
    NavigationServer::get_singleton()->agent_set_time_horizon(agent, 0.0);
    NavigationServer::get_singleton()->agent_set_radius(agent, radius);
    NavigationServer::get_singleton()->agent_set_max_speed(agent, 0.0);
}

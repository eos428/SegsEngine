/*************************************************************************/
/*  cpu_particles_3d.cpp                                                 */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "cpu_particles_3d.h"

#include "core/callable_method_pointer.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/resources/particles_material.h"
#include "scene/resources/curve_texture.h"
#include "scene/resources/mesh.h"
#include "servers/rendering_server.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/mutex.h"
#include "core/translation_helpers.h"

IMPL_GDCLASS(CPUParticles3D)
VARIANT_ENUM_CAST(CPUParticles3D::DrawOrder)
VARIANT_ENUM_CAST(CPUParticles3D::Parameter)
VARIANT_ENUM_CAST(CPUParticles3D::Flags)
VARIANT_ENUM_CAST(CPUParticles3D::EmissionShape)

AABB CPUParticles3D::get_aabb() const {
    return AABB();
}

Vector<Face3> CPUParticles3D::get_faces(uint32_t p_usage_flags) const {
    return Vector<Face3>();
}

void CPUParticles3D::set_emitting(bool p_emitting) {
    if (emitting == p_emitting) {
        return;
    }

    emitting = p_emitting;
    if (emitting) {
        set_process_internal(true);

        // first update before rendering to avoid one frame delay after emitting starts
        if (time == 0.0f) {
            _update_internal();
        }
    }
}

void CPUParticles3D::set_amount(int p_amount) {
    ERR_FAIL_COND_MSG(p_amount < 1, "Amount of particles must be greater than 0.");

    particles.resize(p_amount);
    {
        PoolVector<Particle>::Write w = particles.write();

        for (int i = 0; i < p_amount; i++) {
            w[i].active = false;
            w[i].custom[3] = 0.0; // Make sure w component isn't garbage data
        }
    }

    particle_data.resize((12 + 4 + 1) * p_amount);
    RenderingServer::get_singleton()->multimesh_allocate(multimesh, p_amount, RS::MULTIMESH_TRANSFORM_3D, RS::MULTIMESH_COLOR_8BIT, RS::MULTIMESH_CUSTOM_DATA_FLOAT);

    particle_order.resize(p_amount);
}
void CPUParticles3D::set_lifetime(float p_lifetime) {

    ERR_FAIL_COND_MSG(p_lifetime <= 0, "Particles lifetime must be greater than 0.");
    lifetime = p_lifetime;
}

void CPUParticles3D::set_one_shot(bool p_one_shot) {

    one_shot = p_one_shot;
}

void CPUParticles3D::set_pre_process_time(float p_time) {

    pre_process_time = p_time;
}
void CPUParticles3D::set_explosiveness_ratio(float p_ratio) {

    explosiveness_ratio = p_ratio;
}
void CPUParticles3D::set_randomness_ratio(float p_ratio) {

    randomness_ratio = p_ratio;
}
void CPUParticles3D::set_lifetime_randomness(float p_random) {

    lifetime_randomness = p_random;
}
void CPUParticles3D::set_use_local_coordinates(bool p_enable) {

    local_coords = p_enable;
}
void CPUParticles3D::set_speed_scale(float p_scale) {

    speed_scale = p_scale;
}

bool CPUParticles3D::is_emitting() const {

    return emitting;
}
int CPUParticles3D::get_amount() const {

    return particles.size();
}
float CPUParticles3D::get_lifetime() const {

    return lifetime;
}
bool CPUParticles3D::get_one_shot() const {

    return one_shot;
}

float CPUParticles3D::get_pre_process_time() const {

    return pre_process_time;
}
float CPUParticles3D::get_explosiveness_ratio() const {

    return explosiveness_ratio;
}
float CPUParticles3D::get_randomness_ratio() const {

    return randomness_ratio;
}
float CPUParticles3D::get_lifetime_randomness() const {

    return lifetime_randomness;
}

bool CPUParticles3D::get_use_local_coordinates() const {

    return local_coords;
}

float CPUParticles3D::get_speed_scale() const {

    return speed_scale;
}

void CPUParticles3D::set_draw_order(DrawOrder p_order) {

    draw_order = p_order;
}

CPUParticles3D::DrawOrder CPUParticles3D::get_draw_order() const {

    return draw_order;
}

void CPUParticles3D::set_mesh(const Ref<Mesh> &p_mesh) {

    mesh = p_mesh;
    if (mesh) {
        RenderingServer::get_singleton()->multimesh_set_mesh(multimesh, mesh->get_rid());
    } else {
        RenderingServer::get_singleton()->multimesh_set_mesh(multimesh, RID());
    }
}

Ref<Mesh> CPUParticles3D::get_mesh() const {

    return mesh;
}

void CPUParticles3D::set_fixed_fps(int p_count) {
    fixed_fps = p_count;
}

int CPUParticles3D::get_fixed_fps() const {
    return fixed_fps;
}

void CPUParticles3D::set_fractional_delta(bool p_enable) {
    fractional_delta = p_enable;
}

bool CPUParticles3D::get_fractional_delta() const {
    return fractional_delta;
}

StringName CPUParticles3D::get_configuration_warning() const {

    String warnings;

    bool mesh_found = false;
    bool anim_material_found = false;

    if (get_mesh()) {
        mesh_found = true;
        for (int j = 0; j < get_mesh()->get_surface_count(); j++) {
            anim_material_found = object_cast<ShaderMaterial>(get_mesh()->surface_get_material(j).get()) != nullptr;
            SpatialMaterial *spat = object_cast<SpatialMaterial>(get_mesh()->surface_get_material(j).get());
            anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == SpatialMaterial::BILLBOARD_PARTICLES);
        }
    }

    anim_material_found = anim_material_found || object_cast<ShaderMaterial>(get_material_override().get()) != nullptr;
    SpatialMaterial *spat = object_cast<SpatialMaterial>(get_material_override().get());
    anim_material_found = anim_material_found || (spat && spat->get_billboard_mode() == SpatialMaterial::BILLBOARD_PARTICLES);

    if (!mesh_found) {
        if (!warnings.empty()) {
            warnings += '\n';
        }
        warnings += "- " + TTR("Nothing is visible because no mesh has been assigned.");
    }

    if (!anim_material_found && (get_param(PARAM_ANIM_SPEED) != 0.0 || get_param(PARAM_ANIM_OFFSET) != 0.0 ||
                                        get_param_curve(PARAM_ANIM_SPEED) || get_param_curve(PARAM_ANIM_OFFSET))) {
        if (!warnings.empty()) {
            warnings += '\n';
        }
        warnings += "- " + TTR("CPUParticles3D animation requires the usage of a SpatialMaterial whose Billboard Mode is set to \"Particle Billboard\".");
    }

    return StringName(warnings);
}

void CPUParticles3D::restart() {
    time = 0;
    inactive_time = 0;
    frame_remainder = 0;
    cycle = 0;
    emitting = false;

    {
        int pc = particles.size();
        PoolVector<Particle>::Write w = particles.write();

        for (int i = 0; i < pc; i++) {
            w[i].active = false;
        }
    }
    set_emitting(true);
}

void CPUParticles3D::set_direction(Vector3 p_direction) {

    direction = p_direction;
}

Vector3 CPUParticles3D::get_direction() const {

    return direction;
}

void CPUParticles3D::set_spread(float p_spread) {

    spread = p_spread;
}

float CPUParticles3D::get_spread() const {

    return spread;
}

void CPUParticles3D::set_flatness(float p_flatness) {

    flatness = p_flatness;
}
float CPUParticles3D::get_flatness() const {

    return flatness;
}

void CPUParticles3D::set_param(Parameter p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    parameters[p_param] = p_value;
}
float CPUParticles3D::get_param(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

    return parameters[p_param];
}

void CPUParticles3D::set_param_randomness(Parameter p_param, float p_value) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    randomness[p_param] = p_value;
}
float CPUParticles3D::get_param_randomness(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);

    return randomness[p_param];
}

void CPUParticles3D::set_param_curve(Parameter p_param, const Ref<Curve> &p_curve) {

    ERR_FAIL_INDEX(p_param, PARAM_MAX);

    curve_parameters[p_param] = p_curve;
    const CurveRange range_to_set = c_default_curve_ranges[p_param];
    if(p_curve)
        p_curve->ensure_default_setup(range_to_set.curve_min,range_to_set.curve_max);
}
Ref<Curve> CPUParticles3D::get_param_curve(Parameter p_param) const {

    ERR_FAIL_INDEX_V(p_param, PARAM_MAX, Ref<Curve>());

    return curve_parameters[p_param];
}

void CPUParticles3D::set_color(const Color &p_color) {

    color = p_color;
}

Color CPUParticles3D::get_color() const {

    return color;
}

void CPUParticles3D::set_color_ramp(const Ref<Gradient> &p_ramp) {

    color_ramp = p_ramp;
}

Ref<Gradient> CPUParticles3D::get_color_ramp() const {

    return color_ramp;
}

void CPUParticles3D::set_particle_flag(Flags p_flag, bool p_enable) {
    ERR_FAIL_INDEX(p_flag, FLAG_MAX);
    flags[p_flag] = p_enable;
    if (p_flag == FLAG_DISABLE_Z) {
        Object_change_notify(this);
    }
}

bool CPUParticles3D::get_particle_flag(Flags p_flag) const {
    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
    return flags[p_flag];
}

void CPUParticles3D::set_emission_shape(EmissionShape p_shape) {
    ERR_FAIL_INDEX(p_shape, EMISSION_SHAPE_MAX);

    emission_shape = p_shape;
}

void CPUParticles3D::set_emission_sphere_radius(float p_radius) {

    emission_sphere_radius = p_radius;
}

void CPUParticles3D::set_emission_box_extents(Vector3 p_extents) {

    emission_box_extents = p_extents;
}

void CPUParticles3D::set_emission_points(const PoolVector<Vector3> &p_points) {

    emission_points = p_points;
}

void CPUParticles3D::set_emission_normals(const PoolVector<Vector3> &p_normals) {

    emission_normals = p_normals;
}

void CPUParticles3D::set_emission_colors(const PoolVector<Color> &p_colors) {

    emission_colors = p_colors;
}

float CPUParticles3D::get_emission_sphere_radius() const {

    return emission_sphere_radius;
}
Vector3 CPUParticles3D::get_emission_box_extents() const {

    return emission_box_extents;
}
PoolVector<Vector3> CPUParticles3D::get_emission_points() const {

    return emission_points;
}
PoolVector<Vector3> CPUParticles3D::get_emission_normals() const {

    return emission_normals;
}

PoolVector<Color> CPUParticles3D::get_emission_colors() const {

    return emission_colors;
}

CPUParticles3D::EmissionShape CPUParticles3D::get_emission_shape() const {
    return emission_shape;
}
void CPUParticles3D::set_gravity(const Vector3 &p_gravity) {

    gravity = p_gravity;
}

Vector3 CPUParticles3D::get_gravity() const {

    return gravity;
}

void CPUParticles3D::_validate_property(PropertyInfo &property) const {

    if (property.name == "color" && color_ramp) {
        property.usage = 0;
    }

    if (property.name == "emission_sphere_radius" && emission_shape != EMISSION_SHAPE_SPHERE) {
        property.usage = 0;
    }

    if (property.name == "emission_box_extents" && emission_shape != EMISSION_SHAPE_BOX) {
        property.usage = 0;
    }

    if ((property.name == "emission_point_texture" || property.name == "emission_color_texture") && (emission_shape < EMISSION_SHAPE_POINTS)) {
        property.usage = 0;
    }

    if (property.name == "emission_normals" && emission_shape != EMISSION_SHAPE_DIRECTED_POINTS) {
        property.usage = 0;
    }

    if (StringUtils::begins_with(property.name,"orbit_") && !flags[FLAG_DISABLE_Z]) {
        property.usage = 0;
    }
}

void CPUParticles3D::_update_internal() {

    if (particles.empty() || !is_visible_in_tree()) {
        _set_redraw(false);
        return;
    }

    float delta = get_process_delta_time();
    if (emitting) {
        inactive_time = 0;
    } else {
        inactive_time += delta;
        if (inactive_time > lifetime * 1.2f) {
            set_process_internal(false);
            _set_redraw(false);

            //reset variables
            time = 0;
            inactive_time = 0;
            frame_remainder = 0;
            cycle = 0;
            return;
        }
    }
    _set_redraw(true);

    bool processed = false;

    if (time == 0.0f && pre_process_time > 0.0f) {

        float frame_time;
        if (fixed_fps > 0)
            frame_time = 1.0f / fixed_fps;
        else
            frame_time = 1.0f / 30.0f;

        float todo = pre_process_time;

        while (todo >= 0) {
            _particles_process(frame_time);
            processed = true;
            todo -= frame_time;
        }
    }

    if (fixed_fps > 0) {
        float frame_time = 1.0f / fixed_fps;
        float decr = frame_time;

        float ldelta = delta;
        if (ldelta > 0.1f) { //avoid recursive stalls if fps goes below 10
            ldelta = 0.1f;
        } else if (ldelta <= 0.0f) { //unlikely but..
            ldelta = 0.001f;
        }
        float todo = frame_remainder + ldelta;

        while (todo >= frame_time) {
            _particles_process(frame_time);
            processed = true;
            todo -= decr;
        }

        frame_remainder = todo;

    } else {
        _particles_process(delta);
        processed = true;
    }

    if (processed) {
        _update_particle_data_buffer();
    }
}

void CPUParticles3D::_particles_process(float p_delta) {
    using namespace ParticleUtils;

    p_delta *= speed_scale;

    int pcount = particles.size();
    PoolVector<Particle>::Write w = particles.write();

    Particle *parray = w.ptr();

    float prev_time = time;
    time += p_delta;
    if (time > lifetime) {
        time = Math::fmod(time, lifetime);
        cycle++;
        if (one_shot && cycle > 0) {
            set_emitting(false);
            Object_change_notify(this);
        }
    }

    Transform emission_xform;
    Basis velocity_xform;
    if (!local_coords) {
        emission_xform = get_global_transform();
        velocity_xform = emission_xform.basis;
    }

    float system_phase = time / lifetime;

    for (int i = 0; i < pcount; i++) {

        Particle &p = parray[i];

        if (!emitting && !p.active)
            continue;

        float local_delta = p_delta;

        // The phase is a ratio between 0 (birth) and 1 (end of life) for each particle.
        // While we use time in tests later on, for randomness we use the phase as done in the
        // original shader code, and we later multiply by lifetime to get the time.
        float restart_phase = float(i) / float(pcount);

        if (randomness_ratio > 0.0f) {
            uint32_t seed = cycle;
            if (restart_phase >= system_phase) {
                seed -= uint32_t(1);
            }
            seed *= uint32_t(pcount);
            seed += uint32_t(i);
            float random = float(idhash(seed) % uint32_t(65536)) / 65536.0f;
            restart_phase += randomness_ratio * random * 1.0f / float(pcount);
        }

        restart_phase *= (1.0f - explosiveness_ratio);
        float restart_time = restart_phase * lifetime;
        bool restart = false;

        if (time > prev_time) {
            // restart_time >= prev_time is used so particles emit in the first frame they are processed

            if (restart_time >= prev_time && restart_time < time) {
                restart = true;
                if (fractional_delta) {
                    local_delta = time - restart_time;
                }
            }

        } else if (local_delta > 0.0f) {
            if (restart_time >= prev_time) {
                restart = true;
                if (fractional_delta) {
                    local_delta = lifetime - restart_time + time;
                }

            } else if (restart_time < time) {
                restart = true;
                if (fractional_delta) {
                    local_delta = time - restart_time;
                }
            }
        }

        if (p.time * (1.0f - explosiveness_ratio) > p.lifetime) {
            restart = true;
        }

        if (restart) {

            if (!emitting) {
                p.active = false;
                continue;
            }
            p.active = true;

            /*float tex_linear_velocity = 0;
            if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]) {
                tex_linear_velocity = curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]->interpolate(0);
            }*/

            float tex_angle = 0.0;
            if (curve_parameters[PARAM_ANGLE]) {
                tex_angle = curve_parameters[PARAM_ANGLE]->interpolate(0);
            }

            float tex_anim_offset = 0.0;
            if (curve_parameters[PARAM_ANGLE]) {
                tex_anim_offset = curve_parameters[PARAM_ANGLE]->interpolate(0);
            }

            p.seed = Math::rand();

            p.angle_rand = Math::randf();
            p.scale_rand = Math::randf();
            p.hue_rot_rand = Math::randf();
            p.anim_offset_rand = Math::randf();

            if (flags[FLAG_DISABLE_Z]) {
                float angle1_rad = Math::atan2(direction.y, direction.x) + (Math::randf() * 2.0f - 1.0f) * Math_PI * spread / 180.0f;
                Vector3 rot = Vector3(Math::cos(angle1_rad), Math::sin(angle1_rad), 0.0);
                p.velocity = rot * parameters[PARAM_INITIAL_LINEAR_VELOCITY] * Math::lerp(1.0f, float(Math::randf()), randomness[PARAM_INITIAL_LINEAR_VELOCITY]);
            } else {
                //initiate velocity spread in 3D
                float angle1_rad = Math::atan2(direction.x, direction.z) + (Math::randf() * 2.0f - 1.0f) * Math_PI * spread / 180.0f;
                float angle2_rad = Math::atan2(direction.y, Math::abs(direction.z)) + (Math::randf() * 2.0f - 1.0f) * (1.0f - flatness) * Math_PI * spread / 180.0f;

                Vector3 direction_xz = Vector3(Math::sin(angle1_rad), 0, Math::cos(angle1_rad));
                Vector3 direction_yz = Vector3(0, Math::sin(angle2_rad), Math::cos(angle2_rad));
                direction_yz.z = direction_yz.z / M_MAX(0.0001f, Math::sqrt(ABS(direction_yz.z))); //better uniform distribution
                Vector3 direction = Vector3(direction_xz.x * direction_yz.z, direction_yz.y, direction_xz.z * direction_yz.z);
                direction.normalize();
                p.velocity = direction * parameters[PARAM_INITIAL_LINEAR_VELOCITY] * Math::lerp(1.0f, float(Math::randf()), randomness[PARAM_INITIAL_LINEAR_VELOCITY]);
            }

            float base_angle = (parameters[PARAM_ANGLE] + tex_angle) * Math::lerp(1.0f, p.angle_rand, randomness[PARAM_ANGLE]);
            p.custom[0] = Math::deg2rad(base_angle); //angle
            p.custom[1] = 0.0; //phase
            p.custom[2] = (parameters[PARAM_ANIM_OFFSET] + tex_anim_offset) * Math::lerp(1.0f, p.anim_offset_rand, randomness[PARAM_ANIM_OFFSET]); //animation offset (0-1)
            p.transform = Transform();
            p.time = 0;
            p.lifetime = lifetime * (1.0f - Math::randf() * lifetime_randomness);
            p.base_color = Color(1, 1, 1, 1);

            switch (emission_shape) {
                case EMISSION_SHAPE_POINT: {
                    //do none
                } break;
                case EMISSION_SHAPE_SPHERE: {
                    float s = 2.0 * Math::randf() - 1.0f, t = 2.0f * Math_PI * Math::randf();
                    float radius = emission_sphere_radius * Math::sqrt(1.0f - s * s);
                    p.transform.origin = Vector3(radius * Math::cos(t), radius * Math::sin(t), emission_sphere_radius * s);
                } break;
                case EMISSION_SHAPE_BOX: {
                    p.transform.origin = Vector3(Math::randf() * 2.0 - 1.0, Math::randf() * 2.0 - 1.0, Math::randf() * 2.0 - 1.0) * emission_box_extents;
                } break;
                case EMISSION_SHAPE_POINTS:
                case EMISSION_SHAPE_DIRECTED_POINTS: {

                    int pc = emission_points.size();
                    if (pc == 0)
                        break;

                    int random_idx = Math::rand() % pc;

                    p.transform.origin = emission_points.get(random_idx);

                    if (emission_shape == EMISSION_SHAPE_DIRECTED_POINTS && emission_normals.size() == pc) {
                        if (flags[FLAG_DISABLE_Z]) {
                            Vector3 normal = emission_normals.get(random_idx);
                            Vector2 normal_2d(normal.x, normal.y);
                            Transform2D m2;
                            m2.set_axis(0, normal_2d);
                            m2.set_axis(1, normal_2d.tangent());
                            Vector2 velocity_2d(p.velocity.x, p.velocity.y);
                            velocity_2d = m2.basis_xform(velocity_2d);
                            p.velocity.x = velocity_2d.x;
                            p.velocity.y = velocity_2d.y;
                        } else {
                            Vector3 normal = emission_normals.get(random_idx);
                            Vector3 v0 = Math::abs(normal.z) < 0.999f ? Vector3(0.0, 0.0, 1.0) : Vector3(0, 1.0, 0.0);
                            Vector3 tangent = v0.cross(normal).normalized();
                            Vector3 bitangent = tangent.cross(normal).normalized();
                            Basis m3;
                            m3.set_axis(0, tangent);
                            m3.set_axis(1, bitangent);
                            m3.set_axis(2, normal);
                            p.velocity = m3.xform(p.velocity);
                        }
                    }

                    if (emission_colors.size() == pc) {
                        p.base_color = emission_colors.get(random_idx);
                    }
                } break;
            case EMISSION_SHAPE_MAX: { // Max value for validity check.
                break;
            }
            }

            if (!local_coords) {
                p.velocity = velocity_xform.xform(p.velocity);
                p.transform = emission_xform * p.transform;
            }

            if (flags[FLAG_DISABLE_Z]) {
                p.velocity.z = 0.0;
                p.transform.origin.z = 0.0;
            }

        } else if (!p.active) {
            continue;
        } else if (p.time > p.lifetime) {
            p.active = false;
        } else {

            uint32_t alt_seed = p.seed;

            p.time += local_delta;
            p.custom[1] = p.time / lifetime;

            float tex_linear_velocity = 0.0;
            if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]) {
                tex_linear_velocity = curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]->interpolate(p.custom[1]);
            }

            float tex_orbit_velocity = 0.0;
            if (flags[FLAG_DISABLE_Z]) {
                if (curve_parameters[PARAM_ORBIT_VELOCITY]) {
                    tex_orbit_velocity = curve_parameters[PARAM_ORBIT_VELOCITY]->interpolate(p.custom[1]);
                }
            }

            float tex_angular_velocity = 0.0;
            if (curve_parameters[PARAM_ANGULAR_VELOCITY]) {
                tex_angular_velocity = curve_parameters[PARAM_ANGULAR_VELOCITY]->interpolate(p.custom[1]);
            }

            float tex_linear_accel = 0.0;
            if (curve_parameters[PARAM_LINEAR_ACCEL]) {
                tex_linear_accel = curve_parameters[PARAM_LINEAR_ACCEL]->interpolate(p.custom[1]);
            }

            float tex_tangential_accel = 0.0;
            if (curve_parameters[PARAM_TANGENTIAL_ACCEL]) {
                tex_tangential_accel = curve_parameters[PARAM_TANGENTIAL_ACCEL]->interpolate(p.custom[1]);
            }

            float tex_radial_accel = 0.0;
            if (curve_parameters[PARAM_RADIAL_ACCEL]) {
                tex_radial_accel = curve_parameters[PARAM_RADIAL_ACCEL]->interpolate(p.custom[1]);
            }

            float tex_damping = 0.0;
            if (curve_parameters[PARAM_DAMPING]) {
                tex_damping = curve_parameters[PARAM_DAMPING]->interpolate(p.custom[1]);
            }

            float tex_angle = 0.0;
            if (curve_parameters[PARAM_ANGLE]) {
                tex_angle = curve_parameters[PARAM_ANGLE]->interpolate(p.custom[1]);
            }
            float tex_anim_speed = 0.0;
            if (curve_parameters[PARAM_ANIM_SPEED]) {
                tex_anim_speed = curve_parameters[PARAM_ANIM_SPEED]->interpolate(p.custom[1]);
            }

            float tex_anim_offset = 0.0;
            if (curve_parameters[PARAM_ANIM_OFFSET]) {
                tex_anim_offset = curve_parameters[PARAM_ANIM_OFFSET]->interpolate(p.custom[1]);
            }

            Vector3 force = gravity;
            Vector3 position = p.transform.origin;
            if (flags[FLAG_DISABLE_Z]) {
                position.z = 0.0;
            }
            //apply linear acceleration
            force += p.velocity.length() > 0.0 ? p.velocity.normalized() * (parameters[PARAM_LINEAR_ACCEL] + tex_linear_accel) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_LINEAR_ACCEL]) : Vector3();
            //apply radial acceleration
            Vector3 org = emission_xform.origin;
            Vector3 diff = position - org;
            force += diff.length() > 0.0 ? diff.normalized() * (parameters[PARAM_RADIAL_ACCEL] + tex_radial_accel) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_RADIAL_ACCEL]) : Vector3();
            //apply tangential acceleration;
            if (flags[FLAG_DISABLE_Z]) {

                Vector2 yx = Vector2(diff.y, diff.x);
                Vector2 yx2 = (yx * Vector2(-1.0, 1.0)).normalized();
                force += yx.length() > 0.0 ? Vector3(yx2.x, yx2.y, 0.0) * ((parameters[PARAM_TANGENTIAL_ACCEL] + tex_tangential_accel) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_TANGENTIAL_ACCEL])) : Vector3();

            } else {
                Vector3 crossDiff = diff.normalized().cross(gravity.normalized());
                force += crossDiff.length() > 0.0 ? crossDiff.normalized() * ((parameters[PARAM_TANGENTIAL_ACCEL] + tex_tangential_accel) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_TANGENTIAL_ACCEL])) : Vector3();
            }
            //apply attractor forces
            p.velocity += force * local_delta;
            //orbit velocity
            if (flags[FLAG_DISABLE_Z]) {
                float orbit_amount = (parameters[PARAM_ORBIT_VELOCITY] + tex_orbit_velocity) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_ORBIT_VELOCITY]);
                if (orbit_amount != 0.0) {
                    float ang = orbit_amount * local_delta * Math_PI * 2.0f;
                    // Not sure why the ParticlesMaterial code uses a clockwise rotation matrix,
                    // but we use -ang here to reproduce its behavior.
                    Transform2D rot = Transform2D(-ang, Vector2());
                    Vector2 rotv = rot.basis_xform(Vector2(diff.x, diff.y));
                    p.transform.origin -= Vector3(diff.x, diff.y, 0);
                    p.transform.origin += Vector3(rotv.x, rotv.y, 0);
                }
            }
            if (curve_parameters[PARAM_INITIAL_LINEAR_VELOCITY]) {
                p.velocity = p.velocity.normalized() * tex_linear_velocity;
            }
            if (parameters[PARAM_DAMPING] + tex_damping > 0.0f) {

                float v = p.velocity.length();
                float damp = (parameters[PARAM_DAMPING] + tex_damping) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_DAMPING]);
                v -= damp * local_delta;
                if (v < 0.0) {
                    p.velocity = Vector3();
                } else {
                    p.velocity = p.velocity.normalized() * v;
                }
            }
            float base_angle = (parameters[PARAM_ANGLE] + tex_angle) * Math::lerp(1.0f, p.angle_rand, randomness[PARAM_ANGLE]);
            base_angle += p.custom[1] * lifetime * (parameters[PARAM_ANGULAR_VELOCITY] + tex_angular_velocity) * Math::lerp(1.0f, rand_from_seed(alt_seed) * 2.0f - 1.0f, randomness[PARAM_ANGULAR_VELOCITY]);
            p.custom[0] = Math::deg2rad(base_angle); //angle
            p.custom[2] = (parameters[PARAM_ANIM_OFFSET] + tex_anim_offset) * Math::lerp(1.0f, p.anim_offset_rand, randomness[PARAM_ANIM_OFFSET]) + p.custom[1] * (parameters[PARAM_ANIM_SPEED] + tex_anim_speed) * Math::lerp(1.0f, rand_from_seed(alt_seed), randomness[PARAM_ANIM_SPEED]); //angle
        }
        //apply color
        //apply hue rotation

        float tex_scale = 1.0;
        if (curve_parameters[PARAM_SCALE]) {
            tex_scale = curve_parameters[PARAM_SCALE]->interpolate(p.custom[1]);
        }

        float tex_hue_variation = 0.0;
        if (curve_parameters[PARAM_HUE_VARIATION]) {
            tex_hue_variation = curve_parameters[PARAM_HUE_VARIATION]->interpolate(p.custom[1]);
        }

        float hue_rot_angle = (parameters[PARAM_HUE_VARIATION] + tex_hue_variation) * Math_PI * 2.0 * Math::lerp(1.0f, p.hue_rot_rand * 2.0f - 1.0f, randomness[PARAM_HUE_VARIATION]);
        float hue_rot_c = Math::cos(hue_rot_angle);
        float hue_rot_s = Math::sin(hue_rot_angle);

        Basis hue_rot_mat;
        {
            Basis mat1(0.299f, 0.587f, 0.114f, 0.299f, 0.587f, 0.114f, 0.299f, 0.587f, 0.114f);
            Basis mat2(0.701f, -0.587f, -0.114f, -0.299f, 0.413f, -0.114f, -0.300f, -0.588f, 0.886f);
            Basis mat3(0.168f, 0.330f, -0.497f, -0.328f, 0.035f, 0.292f, 1.250f, -1.050f, -0.203f);

            for (int j = 0; j < 3; j++) {
                hue_rot_mat[j] = mat1[j] + mat2[j] * hue_rot_c + mat3[j] * hue_rot_s;
            }
        }

        if (color_ramp) {
            p.color = color_ramp->get_color_at_offset(p.custom[1]) * color;
        } else {
            p.color = color;
        }

        Vector3 color_rgb = hue_rot_mat.xform_inv(Vector3(p.color.r, p.color.g, p.color.b));
        p.color.r = color_rgb.x;
        p.color.g = color_rgb.y;
        p.color.b = color_rgb.z;

        p.color *= p.base_color;

        if (flags[FLAG_DISABLE_Z]) {

            if (flags[FLAG_ALIGN_Y_TO_VELOCITY]) {
                if (p.velocity.length() > 0.0) {
                    p.transform.basis.set_axis(1, p.velocity.normalized());
                } else {
                    p.transform.basis.set_axis(1, p.transform.basis.get_axis(1));
                }
                p.transform.basis.set_axis(0, p.transform.basis.get_axis(1).cross(p.transform.basis.get_axis(2)).normalized());
                p.transform.basis.set_axis(2, Vector3(0, 0, 1));

            } else {
                p.transform.basis.set_axis(0, Vector3(Math::cos(p.custom[0]), -Math::sin(p.custom[0]), 0.0));
                p.transform.basis.set_axis(1, Vector3(Math::sin(p.custom[0]), Math::cos(p.custom[0]), 0.0));
                p.transform.basis.set_axis(2, Vector3(0, 0, 1));
            }

        } else {
            //orient particle Y towards velocity
            if (flags[FLAG_ALIGN_Y_TO_VELOCITY]) {
                if (p.velocity.length() > 0.0) {
                    p.transform.basis.set_axis(1, p.velocity.normalized());
                } else {
                    p.transform.basis.set_axis(1, p.transform.basis.get_axis(1).normalized());
                }
                if (p.transform.basis.get_axis(1) == p.transform.basis.get_axis(0)) {
                    p.transform.basis.set_axis(0, p.transform.basis.get_axis(1).cross(p.transform.basis.get_axis(2)).normalized());
                    p.transform.basis.set_axis(2, p.transform.basis.get_axis(0).cross(p.transform.basis.get_axis(1)).normalized());
                } else {
                    p.transform.basis.set_axis(2, p.transform.basis.get_axis(0).cross(p.transform.basis.get_axis(1)).normalized());
                    p.transform.basis.set_axis(0, p.transform.basis.get_axis(1).cross(p.transform.basis.get_axis(2)).normalized());
                }
            } else {
                p.transform.basis.orthonormalize();
            }

            //turn particle by rotation in Y
            if (flags[FLAG_ROTATE_Y]) {
                Basis rot_y(Vector3(0, 1, 0), p.custom[0]);
                p.transform.basis = p.transform.basis * rot_y;
            }
        }

        //scale by scale
        float base_scale = Math::lerp(parameters[PARAM_SCALE] * tex_scale, 1.0f, p.scale_rand * randomness[PARAM_SCALE]);
        if (base_scale == 0.0)
            base_scale = 0.000001f;

        p.transform.basis.scale(Vector3(1, 1, 1) * base_scale);

        if (flags[FLAG_DISABLE_Z]) {
            p.velocity.z = 0.0;
            p.transform.origin.z = 0.0;
        }

        p.transform.origin += p.velocity * local_delta;
    }
}

void CPUParticles3D::_update_particle_data_buffer() {
    MutexLock guard(*update_mutex);

    int pc = particles.size();

    PoolVector<int>::Write ow;
    int *order = nullptr;

    PoolVector<float>::Write w = particle_data.write();
    PoolVector<Particle>::Read r = particles.read();
    float *ptr = w.ptr();

    if (draw_order != DRAW_ORDER_INDEX) {
      ow = particle_order.write();
      order = ow.ptr();

      for (int i = 0; i < pc; i++) {
        order[i] = i;
      }
      if (draw_order == DRAW_ORDER_LIFETIME) {
        SortArray<int, SortLifetime> sorter;
        sorter.compare.particles = r.ptr();
        sorter.sort(order, pc);
      } else if (draw_order == DRAW_ORDER_VIEW_DEPTH) {
        Camera3D *c = get_viewport()->get_camera();
        if (c) {
          Vector3 dir = c->get_global_transform().basis.get_axis(2); //far away to close

          if (local_coords) {

            // will look different from Particles in editor as this is based on the camera in the scenetree
            // and not the editor camera
            dir = inv_emission_transform.xform(dir).normalized();
          } else {
            dir = dir.normalized();
          }

          SortArray<int, SortAxis> sorter;
          sorter.compare.particles = r.ptr();
          sorter.compare.axis = dir;
          sorter.sort(order, pc);
        }
      }
    }

    for (int i = 0; i < pc; i++) {

      int idx = order ? order[i] : i;

      Transform t = r[idx].transform;

      if (!local_coords) {
        t = inv_emission_transform * t;
      }

      if (r[idx].active) {
        ptr[0] = t.basis.elements[0][0];
        ptr[1] = t.basis.elements[0][1];
        ptr[2] = t.basis.elements[0][2];
        ptr[3] = t.origin.x;
        ptr[4] = t.basis.elements[1][0];
        ptr[5] = t.basis.elements[1][1];
        ptr[6] = t.basis.elements[1][2];
        ptr[7] = t.origin.y;
        ptr[8] = t.basis.elements[2][0];
        ptr[9] = t.basis.elements[2][1];
        ptr[10] = t.basis.elements[2][2];
        ptr[11] = t.origin.z;
      } else {
        memset(ptr, 0, sizeof(float) * 12);
      }

      Color c = r[idx].color;
      uint8_t *data8 = (uint8_t *)&ptr[12];
      data8[0] = CLAMP(c.r * 255.0f, 0, 255);
      data8[1] = CLAMP(c.g * 255.0f, 0, 255);
      data8[2] = CLAMP(c.b * 255.0f, 0, 255);
      data8[3] = CLAMP(c.a * 255.0f, 0, 255);

      ptr[13] = r[idx].custom[0];
      ptr[14] = r[idx].custom[1];
      ptr[15] = r[idx].custom[2];
      ptr[16] = r[idx].custom[3];

      ptr += 17;
    }

    can_update = true;
}

void CPUParticles3D::_set_redraw(bool p_redraw) {
    if (redraw == p_redraw)
        return;
    redraw = p_redraw;
    MutexLock guard(*update_mutex);

    auto RS = RenderingServer::get_singleton();
    if (redraw) {
        RS->connect("frame_pre_draw",callable_mp(this, &ClassName::_update_render_thread));
        RS->instance_geometry_set_flag(get_instance(), RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE, true);
        RS->multimesh_set_visible_instances(multimesh, -1);
    } else {
        if(RS->is_connected("frame_pre_draw",callable_mp(this, &ClassName::_update_render_thread))) {
            RS->disconnect("frame_pre_draw",callable_mp(this, &ClassName::_update_render_thread));
        }
        RS->instance_geometry_set_flag(get_instance(), RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE, false);
        RS->multimesh_set_visible_instances(multimesh, 0);
    }
}

void CPUParticles3D::_update_render_thread() {

    MutexLock guard(*update_mutex);
    if (can_update) {
        RenderingServer::get_singleton()->multimesh_set_as_bulk_array(multimesh, particle_data);
        can_update = false; //wait for next time
    }

}

void CPUParticles3D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {
        set_process_internal(emitting);

        // first update before rendering to avoid one frame delay after emitting starts
        if (emitting && (time == 0.0f))
            _update_internal();
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {
        _set_redraw(false);
    }

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
        // first update before rendering to avoid one frame delay after emitting starts
        if (emitting && (time == 0.0f))
            _update_internal();
    }

    if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
        _update_internal();
    }
    if (p_what != NOTIFICATION_TRANSFORM_CHANGED)
      return;

    inv_emission_transform = get_global_transform().affine_inverse();

    if (local_coords)
      return;

    int pc = particles.size();

    PoolVector<float>::Write w = particle_data.write();
    PoolVector<Particle>::Read r = particles.read();
    float *ptr = w.ptr();

    for (int i = 0; i < pc; i++) {

      Transform t = inv_emission_transform * r[i].transform;

      if (r[i].active) {
        ptr[0] = t.basis.elements[0][0];
        ptr[1] = t.basis.elements[0][1];
        ptr[2] = t.basis.elements[0][2];
        ptr[3] = t.origin.x;
        ptr[4] = t.basis.elements[1][0];
        ptr[5] = t.basis.elements[1][1];
        ptr[6] = t.basis.elements[1][2];
        ptr[7] = t.origin.y;
        ptr[8] = t.basis.elements[2][0];
        ptr[9] = t.basis.elements[2][1];
        ptr[10] = t.basis.elements[2][2];
        ptr[11] = t.origin.z;
      } else {
        memset(ptr, 0, sizeof(float) * 12);
      }

      ptr += 17;
    }

    can_update = true;
}

void CPUParticles3D::convert_from_particles(Node *p_particles) {

    GPUParticles3D *particles = object_cast<GPUParticles3D>(p_particles);
    ERR_FAIL_COND_MSG(!particles, "Only Particles nodes can be converted to CPUParticles.");

    set_emitting(particles->is_emitting());
    set_amount(particles->get_amount());
    set_lifetime(particles->get_lifetime());
    set_one_shot(particles->get_one_shot());
    set_pre_process_time(particles->get_pre_process_time());
    set_explosiveness_ratio(particles->get_explosiveness_ratio());
    set_randomness_ratio(particles->get_randomness_ratio());
    set_use_local_coordinates(particles->get_use_local_coordinates());
    set_fixed_fps(particles->get_fixed_fps());
    set_fractional_delta(particles->get_fractional_delta());
    set_speed_scale(particles->get_speed_scale());
    set_draw_order(DrawOrder(particles->get_draw_order()));
    set_mesh(particles->get_draw_pass_mesh(0));

    Ref<ParticlesMaterial> material = dynamic_ref_cast<ParticlesMaterial>(particles->get_process_material());
    if (not material)
        return;

    set_direction(material->get_direction());
    set_spread(material->get_spread());
    set_flatness(material->get_flatness());

    set_color(material->get_color());

    Ref<GradientTexture> gt = dynamic_ref_cast<GradientTexture>(material->get_color_ramp());
    if (gt) {
        set_color_ramp(gt->get_gradient());
    }

    set_particle_flag(FLAG_ALIGN_Y_TO_VELOCITY, material->get_flag(ParticlesMaterial::FLAG_ALIGN_Y_TO_VELOCITY));
    set_particle_flag(FLAG_ROTATE_Y, material->get_flag(ParticlesMaterial::FLAG_ROTATE_Y));
    set_particle_flag(FLAG_DISABLE_Z, material->get_flag(ParticlesMaterial::FLAG_DISABLE_Z));

    set_emission_shape(EmissionShape(material->get_emission_shape()));
    set_emission_sphere_radius(material->get_emission_sphere_radius());
    set_emission_box_extents(material->get_emission_box_extents());

    set_gravity(material->get_gravity());
    set_lifetime_randomness(material->get_lifetime_randomness());

#define CONVERT_PARAM(m_param)                                                            \
    set_param(m_param, material->get_param(ParticlesMaterial::m_param));                  \
    {                                                                                     \
        Ref<CurveTexture> ctex = dynamic_ref_cast<CurveTexture>(material->get_param_texture(ParticlesMaterial::m_param)); \
        if (ctex) set_param_curve(m_param, ctex->get_curve());                 \
    }                                                                                     \
    set_param_randomness(m_param, material->get_param_randomness(ParticlesMaterial::m_param));

    CONVERT_PARAM(PARAM_INITIAL_LINEAR_VELOCITY)
    CONVERT_PARAM(PARAM_ANGULAR_VELOCITY)
    CONVERT_PARAM(PARAM_ORBIT_VELOCITY)
    CONVERT_PARAM(PARAM_LINEAR_ACCEL)
    CONVERT_PARAM(PARAM_RADIAL_ACCEL)
    CONVERT_PARAM(PARAM_TANGENTIAL_ACCEL)
    CONVERT_PARAM(PARAM_DAMPING)
    CONVERT_PARAM(PARAM_ANGLE)
    CONVERT_PARAM(PARAM_SCALE)
    CONVERT_PARAM(PARAM_HUE_VARIATION)
    CONVERT_PARAM(PARAM_ANIM_SPEED)
    CONVERT_PARAM(PARAM_ANIM_OFFSET)

#undef CONVERT_PARAM
}

void CPUParticles3D::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_emitting", {"emitting"}), &CPUParticles3D::set_emitting);
    MethodBinder::bind_method(D_METHOD("set_amount", {"amount"}), &CPUParticles3D::set_amount);
    MethodBinder::bind_method(D_METHOD("set_lifetime", {"secs"}), &CPUParticles3D::set_lifetime);
    MethodBinder::bind_method(D_METHOD("set_one_shot", {"enable"}), &CPUParticles3D::set_one_shot);
    MethodBinder::bind_method(D_METHOD("set_pre_process_time", {"secs"}), &CPUParticles3D::set_pre_process_time);
    MethodBinder::bind_method(D_METHOD("set_explosiveness_ratio", {"ratio"}), &CPUParticles3D::set_explosiveness_ratio);
    MethodBinder::bind_method(D_METHOD("set_randomness_ratio", {"ratio"}), &CPUParticles3D::set_randomness_ratio);
    MethodBinder::bind_method(D_METHOD("set_lifetime_randomness", {"random"}), &CPUParticles3D::set_lifetime_randomness);
    MethodBinder::bind_method(D_METHOD("set_use_local_coordinates", {"enable"}), &CPUParticles3D::set_use_local_coordinates);
    MethodBinder::bind_method(D_METHOD("set_fixed_fps", {"fps"}), &CPUParticles3D::set_fixed_fps);
    MethodBinder::bind_method(D_METHOD("set_fractional_delta", {"enable"}), &CPUParticles3D::set_fractional_delta);
    MethodBinder::bind_method(D_METHOD("set_speed_scale", {"scale"}), &CPUParticles3D::set_speed_scale);

    MethodBinder::bind_method(D_METHOD("is_emitting"), &CPUParticles3D::is_emitting);
    MethodBinder::bind_method(D_METHOD("get_amount"), &CPUParticles3D::get_amount);
    MethodBinder::bind_method(D_METHOD("get_lifetime"), &CPUParticles3D::get_lifetime);
    MethodBinder::bind_method(D_METHOD("get_one_shot"), &CPUParticles3D::get_one_shot);
    MethodBinder::bind_method(D_METHOD("get_pre_process_time"), &CPUParticles3D::get_pre_process_time);
    MethodBinder::bind_method(D_METHOD("get_explosiveness_ratio"), &CPUParticles3D::get_explosiveness_ratio);
    MethodBinder::bind_method(D_METHOD("get_randomness_ratio"), &CPUParticles3D::get_randomness_ratio);
    MethodBinder::bind_method(D_METHOD("get_lifetime_randomness"), &CPUParticles3D::get_lifetime_randomness);
    MethodBinder::bind_method(D_METHOD("get_use_local_coordinates"), &CPUParticles3D::get_use_local_coordinates);
    MethodBinder::bind_method(D_METHOD("get_fixed_fps"), &CPUParticles3D::get_fixed_fps);
    MethodBinder::bind_method(D_METHOD("get_fractional_delta"), &CPUParticles3D::get_fractional_delta);
    MethodBinder::bind_method(D_METHOD("get_speed_scale"), &CPUParticles3D::get_speed_scale);

    MethodBinder::bind_method(D_METHOD("set_draw_order", {"order"}), &CPUParticles3D::set_draw_order);

    MethodBinder::bind_method(D_METHOD("get_draw_order"), &CPUParticles3D::get_draw_order);

    MethodBinder::bind_method(D_METHOD("set_mesh", {"mesh"}), &CPUParticles3D::set_mesh);
    MethodBinder::bind_method(D_METHOD("get_mesh"), &CPUParticles3D::get_mesh);

    MethodBinder::bind_method(D_METHOD("restart"), &CPUParticles3D::restart);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "emitting"), "set_emitting", "is_emitting");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "amount", PropertyHint::ExpRange, "1,1000000,1"), "set_amount", "get_amount");
    ADD_GROUP("Time", "");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "lifetime", PropertyHint::ExpRange, "0.01,600.0,0.01,or_greater"), "set_lifetime", "get_lifetime");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "preprocess", PropertyHint::ExpRange, "0.00,600.0,0.01"), "set_pre_process_time", "get_pre_process_time");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "speed_scale", PropertyHint::Range, "0,64,0.01"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "explosiveness", PropertyHint::Range, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "randomness", PropertyHint::Range, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "lifetime_randomness", PropertyHint::Range, "0,1,0.01"), "set_lifetime_randomness", "get_lifetime_randomness");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "fixed_fps", PropertyHint::Range, "0,1000,1"), "set_fixed_fps", "get_fixed_fps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "fract_delta"), "set_fractional_delta", "get_fractional_delta");
    ADD_GROUP("Drawing", "");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "local_coords"), "set_use_local_coordinates", "get_use_local_coordinates");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "draw_order", PropertyHint::Enum, "Index,Lifetime,View Depth"), "set_draw_order", "get_draw_order");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh"), "set_mesh", "get_mesh");

    BIND_ENUM_CONSTANT(DRAW_ORDER_INDEX)
    BIND_ENUM_CONSTANT(DRAW_ORDER_LIFETIME)
    BIND_ENUM_CONSTANT(DRAW_ORDER_VIEW_DEPTH)

    ////////////////////////////////

    MethodBinder::bind_method(D_METHOD("set_direction", {"direction"}), &CPUParticles3D::set_direction);
    MethodBinder::bind_method(D_METHOD("get_direction"), &CPUParticles3D::get_direction);

    MethodBinder::bind_method(D_METHOD("set_spread", {"degrees"}), &CPUParticles3D::set_spread);
    MethodBinder::bind_method(D_METHOD("get_spread"), &CPUParticles3D::get_spread);

    MethodBinder::bind_method(D_METHOD("set_flatness", {"amount"}), &CPUParticles3D::set_flatness);
    MethodBinder::bind_method(D_METHOD("get_flatness"), &CPUParticles3D::get_flatness);

    MethodBinder::bind_method(D_METHOD("set_param", {"param", "value"}), &CPUParticles3D::set_param);
    MethodBinder::bind_method(D_METHOD("get_param", {"param"}), &CPUParticles3D::get_param);

    MethodBinder::bind_method(D_METHOD("set_param_randomness", {"param", "randomness"}), &CPUParticles3D::set_param_randomness);
    MethodBinder::bind_method(D_METHOD("get_param_randomness", {"param"}), &CPUParticles3D::get_param_randomness);

    MethodBinder::bind_method(D_METHOD("set_param_curve", {"param", "curve"}), &CPUParticles3D::set_param_curve);
    MethodBinder::bind_method(D_METHOD("get_param_curve", {"param"}), &CPUParticles3D::get_param_curve);

    MethodBinder::bind_method(D_METHOD("set_color", {"color"}), &CPUParticles3D::set_color);
    MethodBinder::bind_method(D_METHOD("get_color"), &CPUParticles3D::get_color);

    MethodBinder::bind_method(D_METHOD("set_color_ramp", {"ramp"}), &CPUParticles3D::set_color_ramp);
    MethodBinder::bind_method(D_METHOD("get_color_ramp"), &CPUParticles3D::get_color_ramp);

    MethodBinder::bind_method(D_METHOD("set_particle_flag", {"flag", "enable"}), &CPUParticles3D::set_particle_flag);
    MethodBinder::bind_method(D_METHOD("get_particle_flag", {"flag"}), &CPUParticles3D::get_particle_flag);

    MethodBinder::bind_method(D_METHOD("set_emission_shape", {"shape"}), &CPUParticles3D::set_emission_shape);
    MethodBinder::bind_method(D_METHOD("get_emission_shape"), &CPUParticles3D::get_emission_shape);

    MethodBinder::bind_method(D_METHOD("set_emission_sphere_radius", {"radius"}), &CPUParticles3D::set_emission_sphere_radius);
    MethodBinder::bind_method(D_METHOD("get_emission_sphere_radius"), &CPUParticles3D::get_emission_sphere_radius);

    MethodBinder::bind_method(D_METHOD("set_emission_box_extents", {"extents"}), &CPUParticles3D::set_emission_box_extents);
    MethodBinder::bind_method(D_METHOD("get_emission_box_extents"), &CPUParticles3D::get_emission_box_extents);

    MethodBinder::bind_method(D_METHOD("set_emission_points", {"array"}), &CPUParticles3D::set_emission_points);
    MethodBinder::bind_method(D_METHOD("get_emission_points"), &CPUParticles3D::get_emission_points);

    MethodBinder::bind_method(D_METHOD("set_emission_normals", {"array"}), &CPUParticles3D::set_emission_normals);
    MethodBinder::bind_method(D_METHOD("get_emission_normals"), &CPUParticles3D::get_emission_normals);

    MethodBinder::bind_method(D_METHOD("set_emission_colors", {"array"}), &CPUParticles3D::set_emission_colors);
    MethodBinder::bind_method(D_METHOD("get_emission_colors"), &CPUParticles3D::get_emission_colors);

    MethodBinder::bind_method(D_METHOD("get_gravity"), &CPUParticles3D::get_gravity);
    MethodBinder::bind_method(D_METHOD("set_gravity", {"accel_vec"}), &CPUParticles3D::set_gravity);

    MethodBinder::bind_method(D_METHOD("convert_from_particles", {"particles"}), &CPUParticles3D::convert_from_particles);

    MethodBinder::bind_method(D_METHOD("_update_render_thread"), &CPUParticles3D::_update_render_thread);

    ADD_GROUP("Emission Shape", "emission_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "emission_shape", PropertyHint::Enum, "Point,Sphere,Box,Points,Directed Points"), "set_emission_shape", "get_emission_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "emission_sphere_radius", PropertyHint::Range, "0.01,128,0.01"), "set_emission_sphere_radius", "get_emission_sphere_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "emission_box_extents"), "set_emission_box_extents", "get_emission_box_extents");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR3_ARRAY, "emission_points"), "set_emission_points", "get_emission_points");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR3_ARRAY, "emission_normals"), "set_emission_normals", "get_emission_normals");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_COLOR_ARRAY, "emission_colors"), "set_emission_colors", "get_emission_colors");
    ADD_GROUP("Flags", "flag_");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_align_y"), "set_particle_flag", "get_particle_flag", FLAG_ALIGN_Y_TO_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_rotate_y"), "set_particle_flag", "get_particle_flag", FLAG_ROTATE_Y);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "flag_disable_z"), "set_particle_flag", "get_particle_flag", FLAG_DISABLE_Z);
    ADD_GROUP("Direction", "");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "direction"), "set_direction", "get_direction");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "spread", PropertyHint::Range, "0,180,0.01"), "set_spread", "get_spread");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "flatness", PropertyHint::Range, "0,1,0.01"), "set_flatness", "get_flatness");
    ADD_GROUP("Gravity", "");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR3, "gravity"), "set_gravity", "get_gravity");
    ADD_GROUP("Initial Velocity", "initial_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "initial_velocity", PropertyHint::Range, "0,1000,0.01,or_greater"), "set_param", "get_param", PARAM_INITIAL_LINEAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "initial_velocity_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_INITIAL_LINEAR_VELOCITY);
    ADD_GROUP("Angular Velocity", "angular_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angular_velocity", PropertyHint::Range, "-720,720,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_ANGULAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angular_velocity_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANGULAR_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "angular_velocity_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANGULAR_VELOCITY);
    ADD_GROUP("Orbit Velocity", "orbit_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "orbit_velocity", PropertyHint::Range, "-1000,1000,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_ORBIT_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "orbit_velocity_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ORBIT_VELOCITY);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "orbit_velocity_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_ORBIT_VELOCITY);
    ADD_GROUP("Linear Accel", "linear_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "linear_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_LINEAR_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "linear_accel_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_LINEAR_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "linear_accel_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_LINEAR_ACCEL);
    ADD_GROUP("Radial Accel", "radial_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "radial_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_RADIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "radial_accel_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_RADIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "radial_accel_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_RADIAL_ACCEL);
    ADD_GROUP("Tangential Accel", "tangential_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "tangential_accel", PropertyHint::Range, "-100,100,0.01,or_lesser,or_greater"), "set_param", "get_param", PARAM_TANGENTIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "tangential_accel_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_TANGENTIAL_ACCEL);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "tangential_accel_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_TANGENTIAL_ACCEL);
    ADD_GROUP("Damping", "");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "damping", PropertyHint::Range, "0,100,0.01"), "set_param", "get_param", PARAM_DAMPING);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "damping_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_DAMPING);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "damping_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_DAMPING);
    ADD_GROUP("Angle", "");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angle", PropertyHint::Range, "-720,720,0.1,or_lesser,or_greater"), "set_param", "get_param", PARAM_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "angle_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANGLE);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "angle_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANGLE);
    ADD_GROUP("Scale", "");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "scale_amount", PropertyHint::Range, "0,1000,0.01,or_greater"), "set_param", "get_param", PARAM_SCALE);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "scale_amount_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_SCALE);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "scale_amount_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_SCALE);
    ADD_GROUP("Color", "");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "color_ramp", PropertyHint::ResourceType, "Gradient"), "set_color_ramp", "get_color_ramp");

    ADD_GROUP("Hue Variation", "hue_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "hue_variation", PropertyHint::Range, "-1,1,0.01"), "set_param", "get_param", PARAM_HUE_VARIATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "hue_variation_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_HUE_VARIATION);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "hue_variation_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_HUE_VARIATION);
    ADD_GROUP("Animation", "anim_");
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_speed", PropertyHint::Range, "0,128,0.01,or_greater"), "set_param", "get_param", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_speed_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anim_speed_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANIM_SPEED);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_offset", PropertyHint::Range, "0,1,0.01"), "set_param", "get_param", PARAM_ANIM_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, "anim_offset_random", PropertyHint::Range, "0,1,0.01"), "set_param_randomness", "get_param_randomness", PARAM_ANIM_OFFSET);
    ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "anim_offset_curve", PropertyHint::ResourceType, "Curve"), "set_param_curve", "get_param_curve", PARAM_ANIM_OFFSET);

    BIND_ENUM_CONSTANT(PARAM_INITIAL_LINEAR_VELOCITY)
    BIND_ENUM_CONSTANT(PARAM_ANGULAR_VELOCITY)
    BIND_ENUM_CONSTANT(PARAM_ORBIT_VELOCITY)
    BIND_ENUM_CONSTANT(PARAM_LINEAR_ACCEL)
    BIND_ENUM_CONSTANT(PARAM_RADIAL_ACCEL)
    BIND_ENUM_CONSTANT(PARAM_TANGENTIAL_ACCEL)
    BIND_ENUM_CONSTANT(PARAM_DAMPING)
    BIND_ENUM_CONSTANT(PARAM_ANGLE)
    BIND_ENUM_CONSTANT(PARAM_SCALE)
    BIND_ENUM_CONSTANT(PARAM_HUE_VARIATION)
    BIND_ENUM_CONSTANT(PARAM_ANIM_SPEED)
    BIND_ENUM_CONSTANT(PARAM_ANIM_OFFSET)
    BIND_ENUM_CONSTANT(PARAM_MAX)

    BIND_ENUM_CONSTANT(FLAG_ALIGN_Y_TO_VELOCITY)
    BIND_ENUM_CONSTANT(FLAG_ROTATE_Y)
    BIND_ENUM_CONSTANT(FLAG_DISABLE_Z)
    BIND_ENUM_CONSTANT(FLAG_MAX)

    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINT)
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_SPHERE)
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_BOX)
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_POINTS)
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_DIRECTED_POINTS)
    BIND_ENUM_CONSTANT(EMISSION_SHAPE_MAX)
}

CPUParticles3D::CPUParticles3D() {

    time = 0;
    inactive_time = 0;
    frame_remainder = 0;
    cycle = 0;
    redraw = false;
    emitting = false;
    update_mutex = memnew(Mutex);
    set_notify_transform(true);

    multimesh = RenderingServer::get_singleton()->multimesh_create();
    RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
    set_base(multimesh);

    set_emitting(true);
    set_one_shot(false);
    set_amount(8);
    set_lifetime(1);
    set_fixed_fps(0);
    set_fractional_delta(true);
    set_pre_process_time(0);
    set_explosiveness_ratio(0);
    set_randomness_ratio(0);
    set_lifetime_randomness(0);
    set_use_local_coordinates(true);

    set_draw_order(DRAW_ORDER_INDEX);
    set_speed_scale(1);

    set_direction(Vector3(1, 0, 0));
    set_spread(45);
    set_flatness(0);
    set_param(PARAM_INITIAL_LINEAR_VELOCITY, 0);
    set_param(PARAM_ANGULAR_VELOCITY, 0);
    set_param(PARAM_ORBIT_VELOCITY, 0);
    set_param(PARAM_LINEAR_ACCEL, 0);
    set_param(PARAM_RADIAL_ACCEL, 0);
    set_param(PARAM_TANGENTIAL_ACCEL, 0);
    set_param(PARAM_DAMPING, 0);
    set_param(PARAM_ANGLE, 0);
    set_param(PARAM_SCALE, 1);
    set_param(PARAM_HUE_VARIATION, 0);
    set_param(PARAM_ANIM_SPEED, 0);
    set_param(PARAM_ANIM_OFFSET, 0);
    set_emission_shape(EMISSION_SHAPE_POINT);
    set_emission_sphere_radius(1);
    set_emission_box_extents(Vector3(1, 1, 1));

    set_gravity(Vector3(0, -9.8f, 0));

    for (int i = 0; i < PARAM_MAX; i++) {
        set_param_randomness(Parameter(i), 0);
    }

    for (int i = 0; i < FLAG_MAX; i++) {
        flags[i] = false;
    }

    can_update = false;

    set_color(Color(1, 1, 1, 1));
}

CPUParticles3D::~CPUParticles3D() {
    RenderingServer::get_singleton()->free_rid(multimesh);

    memdelete(update_mutex);
}

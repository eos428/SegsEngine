/*************************************************************************/
/*  convex_polygon_shape_2d.h                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef CONVEX_POLYGON_SHAPE_2D_H
#define CONVEX_POLYGON_SHAPE_2D_H

#include "scene/resources/shape_2d.h"

class ConvexPolygonShape2D : public Shape2D {
	GDCLASS(ConvexPolygonShape2D,Shape2D)

	Vector<Vector2> points;
	void _update_shape();

protected:
	static void _bind_methods();

public:
	bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const override;

	void set_point_cloud(const Vector<Vector2> &p_points);
	void set_points(const Vector<Vector2> &p_points);
	Vector<Vector2> get_points() const;

	void draw(const RID &p_to_rid, const Color &p_color) override;
	Rect2 get_rect() const override;

	ConvexPolygonShape2D();
};

#endif // CONVEX_POLYGON_SHAPE_2D_H
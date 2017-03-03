/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2015 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#include <mapnik/debug.hpp>
#include <mapnik/view_transform.hpp>
#include <mapnik/expression_evaluator.hpp>
#include <mapnik/text/point_layout.hpp>
#include <mapnik/text/placements/base.hpp>
#include <mapnik/text/text_layout.hpp>
#include <mapnik/text/glyph_info.hpp>
#include <mapnik/text/text_properties.hpp>
#include <mapnik/text/glyph_positions.hpp>
#include <mapnik/vertex_cache.hpp>
#include <mapnik/util/math.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/marker.hpp>

#include <vector>

namespace mapnik
{

point_layout::point_layout(params_type const & params)
    : params_(params),
      collision_cache_insert_(parse_collision_detector_keys(
          params.get_optional<std::string, keys::collision_cache_insert>())),
      collision_cache_detect_(parse_collision_detector_keys(
          params.get_optional<std::string, keys::collision_cache_detect>()))
{
}

bool point_layout::try_placement(
    text_layout_generator & layout_generator,
    pixel_position const & pos)
{
    glyph_positions_ptr glyphs = std::make_unique<glyph_positions>();
    std::vector<box_type> bboxes;
    layout_container & layouts = *layout_generator.layouts_;
    evaluated_text_properties const & text_props = layout_generator.get_text_props();
    detector_type & detector = layout_generator.detector_;

    // TODO: useful?
    //glyphs->reserve(layouts.glyphs_count());
    //bboxes.reserve(layouts.size());

    if (try_placement(layouts, detector, text_props, pos, *glyphs, bboxes))
    {
        process_bboxes(detector, layouts, glyphs, bboxes);
        return true;
    }

    return false;
}

bool point_layout::try_placement(
    text_layout_generator & layout_generator,
    point_position const & pos)
{
    // TODO: angle
    return try_placement(layout_generator, pos.coords);
}

bool point_layout::try_placement(
    layout_container const & layouts,
    detector_type & detector,
    evaluated_text_properties const & text_props,
    pixel_position const & pos,
    glyph_positions & glyphs,
    std::vector<box_type> & bboxes)
{
    bool base_point_set = false;
    for (auto const& layout_wrapper : layouts)
    {
        text_layout const& layout = layout_wrapper.get();
        rotation const& orientation = layout.orientation();

        // Find text origin.
        pixel_position layout_center = pos + layout.displacement();

        if (!base_point_set)
        {
            glyphs.set_base_point(layout_center);
            base_point_set = true;
        }

        box2d<double> bbox = layout.bounds();
        bbox.re_center(layout_center.x, layout_center.y);

        /* For point placements it is faster to just check the bounding box. */
        if (collision(detector, text_props, bbox, layouts.text())) return false;

        if (layout.glyphs_count()) bboxes.push_back(std::move(bbox));

        pixel_position layout_offset = layout_center - glyphs.get_base_point();
        layout_offset.y = -layout_offset.y;

        // IMPORTANT NOTE:
        //   x and y are relative to the center of the text
        //   coordinate system:
        //   x: grows from left to right
        //   y: grows from bottom to top (opposite of normal computer graphics)

        double x, y;

        // set for upper left corner of text envelope for the first line, top left of first character
        y = layout.height() / 2.0;

        for ( auto const& line : layout)
        {
            y -= line.height(); //Automatically handles first line differently
            x = layout.jalign_offset(line.width());

            for (auto const& glyph : line)
            {
                // place the character relative to the center of the string envelope
                glyphs.emplace_back(glyph, (pixel_position(x, y).rotate(orientation)) + layout_offset, orientation);
                if (glyph.advance())
                {
                    //Only advance if glyph is not part of a multiple glyph sequence
                    x += glyph.advance() + glyph.format->character_spacing * params_.scale_factor;
                }
            }
        }
    }
    return true;
}

void point_layout::process_bboxes(
    detector_type & detector,
    layout_container & layouts,
    glyph_positions_ptr & glyphs,
    std::vector<box_type> const & bboxes)
{
    box_type label_box;
    bool first = true;
    for (auto const & box : bboxes)
    {
        if (first)
        {
            label_box = box;
            first = false;
        }
        else
        {
            label_box.expand_to_include(box);
        }
        detector.insert(box, layouts.text(), collision_cache_insert_);
    }

    // do not render text off the canvas
    // TODO: throw away single glyphs earlier?
    if (params_.dims.intersects(label_box))
    {
        layouts.placements_.emplace_back(std::move(glyphs));
        //glyphs->clear();
    }
}

bool point_layout::collision(
    detector_type & detector,
    evaluated_text_properties const & text_props,
    const box2d<double> &box,
    const value_unicode_string &repeat_key) const
{
    double margin = (text_props.margin != 0 ? text_props.margin : text_props.minimum_distance) * params_.scale_factor;
    double repeat_distance = text_props.repeat_distance * params_.scale_factor;
    return (text_props.avoid_edges && !params_.dims.contains(box))
        ||
        (text_props.minimum_padding > 0 &&
         !params_.dims.contains(box + (params_.scale_factor * text_props.minimum_padding)))
        ||
        (!text_props.allow_overlap &&
         ((repeat_key.length() == 0 && !detector.has_placement(box, margin, collision_cache_detect_))
          ||
          (repeat_key.length() > 0 && !detector.has_placement(box, margin, repeat_key, repeat_distance, collision_cache_detect_))));
}

shield_layout::shield_layout(params_type const & params)
    : point_layout(params),
      marker_unlocked_(mapnik::get<value_bool, keys::unlock_image>(params.symbolizer, params.feature, params.vars)),
      marker_displacement_(pixel_position(
        mapnik::get<value_double, keys::shield_dx>(params.symbolizer, params.feature, params.vars),
        mapnik::get<value_double, keys::shield_dy>(params.symbolizer, params.feature, params.vars)) * params.scale_factor)
{
    std::string filename = mapnik::get<std::string,keys::file>(params.symbolizer, params.feature, params.vars);
    if (filename.empty()) return;
    std::shared_ptr<mapnik::marker const> marker = marker_cache::instance().find(filename, true);
    if (marker->is<marker_null>()) return;
    agg::trans_affine trans;
    auto image_transform = get_optional<transform_type>(params.symbolizer, keys::image_transform);
    if (image_transform) evaluate_transform(trans, params.feature, params.vars, *image_transform, params.scale_factor);
    double width = marker->width();
    double height = marker->height();
    double px0 = - 0.5 * width;
    double py0 = - 0.5 * height;
    double px1 = 0.5 * width;
    double py1 = 0.5 * height;
    double px2 = px1;
    double py2 = py0;
    double px3 = px0;
    double py3 = py1;
    trans.transform(&px0, &py0);
    trans.transform(&px1, &py1);
    trans.transform(&px2, &py2);
    trans.transform(&px3, &py3);
    box2d<double> bbox(px0, py0, px1, py1);
    bbox.expand_to_include(px2, py2);
    bbox.expand_to_include(px3, py3);
    marker_box_ = bbox * params.scale_factor;
    // TODO: shared?
    marker_ = std::make_shared<marker_info>(marker, trans);
}

bool shield_layout::try_placement(
    text_layout_generator & layout_generator,
    pixel_position const& pos)
{
    glyph_positions_ptr glyphs = std::make_unique<glyph_positions>();
    std::vector<box_type> bboxes;
    layout_container & layouts = *layout_generator.layouts_;
    evaluated_text_properties const & text_props = layout_generator.get_text_props();
    detector_type & detector = layout_generator.detector_;

    // TODO: useful?
    //glyphs->reserve(layouts.glyphs_count());
    //bboxes.reserve(layouts.size());

    if (point_layout::try_placement(layouts, detector, text_props, pos, *glyphs, bboxes))
    {
        if (!marker_ || add_marker(detector, layouts, text_props, *glyphs, pos, bboxes))
        {
            process_bboxes(detector, layouts, glyphs, bboxes);
            return true;
        }
    }

    return false;
}

bool shield_layout::try_placement(
    text_layout_generator & layout_generator,
    point_position const & pos)
{
    // TODO: angle
    return try_placement(layout_generator, pos.coords);
}

bool shield_layout::add_marker(
    detector_type & detector,
    layout_container const & layouts,
    evaluated_text_properties const & text_props,
    glyph_positions & glyphs,
    pixel_position const& pos,
    std::vector<box_type> & bboxes) const
{
    pixel_position real_pos = (marker_unlocked_ ? pos :
        glyphs.get_base_point()) + marker_displacement_;
    box2d<double> bbox = marker_box_;
    bbox.move(real_pos.x, real_pos.y);
    if (collision(detector, text_props, bbox, layouts.text())) return false;
    detector.insert(bbox, this->collision_cache_insert_);
    bboxes.push_back(std::move(bbox));
    glyphs.set_marker(marker_, real_pos);
    return true;
}

}// ns mapnik
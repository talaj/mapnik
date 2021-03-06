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

// mapnik
#include <mapnik/group/group_symbolizer_helper.hpp>
#include <mapnik/label_collision_detector.hpp>
#include <mapnik/geometry.hpp>
#include <mapnik/vertex_processor.hpp>
#include <mapnik/debug.hpp>
#include <mapnik/symbolizer.hpp>
#include <mapnik/value_types.hpp>
#include <mapnik/text/placements/base.hpp>
#include <mapnik/text/placements/dummy.hpp>
#include <mapnik/vertex_cache.hpp>
#include <mapnik/tolerance_iterator.hpp>
#include <mapnik/renderer_common.hpp>

namespace mapnik { //namespace detail {

group_layout_generator::group_layout_generator(
    params_type const & params,
    detector_type & detector,
    face_manager_freetype & font_manager,
    text_placement_info & info,
    std::list<box_element> const & box_elements)
    : font_manager_(font_manager),
      text_props_(evaluate_text_properties(info.properties, params.feature, params.vars)),
      box_elements_(box_elements),
      detector_(detector)
{
}

using detector_type = renderer_common::detector_type;

group_point_layout::group_point_layout(params_type const & params)
    : params_(params),
      collision_cache_insert_(parse_collision_detector_keys(
          params.get_optional<std::string, keys::collision_cache_insert>())),
      collision_cache_detect_(parse_collision_detector_keys(
          params.get_optional<std::string, keys::collision_cache_detect>()))
{
}

bool group_point_layout::try_placement(
    group_layout_generator & layout_generator,
    pixel_position const& pos)
{
    std::list<box_element> const & box_elements = layout_generator.box_elements_;
    detector_type & detector = layout_generator.detector_;

    if (box_elements.empty()) return true;

    evaluated_text_properties const & text_props = layout_generator.get_text_props();

    // offset boxes and check collision
    std::list<box2d<double>> real_boxes;
    for (auto const& box_elem : box_elements)
    {
        box2d<double> real_box = box2d<double>(box_elem.box_);
        real_box.move(pos.x, pos.y);
        if (collision(detector, text_props, real_box, box_elem.repeat_key_))
        {
            return false;
        }
        real_boxes.push_back(real_box);
    }

    // add boxes to collision detector
    std::list<box_element>::const_iterator elem_itr = box_elements.cbegin();
    std::list<box2d<double>>::const_iterator real_itr = real_boxes.cbegin();
    while (elem_itr != box_elements.cend() && real_itr != real_boxes.cend())
    {
        detector.insert(*real_itr, elem_itr->repeat_key_, collision_cache_insert_);
        elem_itr++;
        real_itr++;
    }

    layout_generator.placements_.push_back(pos);

    return true;
}

bool group_point_layout::try_placement(
    group_layout_generator & layout_generator,
    point_position const& pos)
{
    // TODO: angle
    return try_placement(layout_generator, pos.coords);
}

bool group_point_layout::collision(
    detector_type & detector,
    evaluated_text_properties const & text_props,
    box2d<double> const& box,
    value_unicode_string const& repeat_key) const
{
    if (!detector.extent().intersects(box)
            ||
        (text_props.avoid_edges && !params_.dims.contains(box))
            ||
        (text_props.minimum_padding > 0 &&
         !params_.dims.contains(box + (params_.scale_factor * text_props.minimum_padding)))
            ||
        (!text_props.allow_overlap &&
            ((repeat_key.length() == 0 && !detector.has_placement(
                box, text_props.margin * params_.scale_factor, collision_cache_detect_))
                ||
             (repeat_key.length() > 0  && !detector.has_placement(
                box, text_props.margin * params_.scale_factor,
                repeat_key, text_props.repeat_distance * params_.scale_factor,
                collision_cache_detect_))))
        )
    {
        return true;
    }
    return false;
}

} //namespace

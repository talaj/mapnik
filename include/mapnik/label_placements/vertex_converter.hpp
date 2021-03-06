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
#ifndef MAPNIK_LABEL_PLACEMENT_VERTEX_CONVERTER_HPP
#define MAPNIK_LABEL_PLACEMENT_VERTEX_CONVERTER_HPP

#include <mapnik/box2d.hpp>
#include <mapnik/util/noncopyable.hpp>
#include <mapnik/vertex_converters.hpp>
#include <mapnik/vertex_adapters.hpp>

namespace mapnik { namespace label_placement {

template <typename VertexConverter>
class set_clip_geometry_visitor
{
public:
    set_clip_geometry_visitor(
        VertexConverter & converter,
        value_bool clip)
        : converter_(converter),
          clip_(clip)
    {
    }

    void operator()(geometry::line_string<double> const & geo) const
    {
        converter_.template set<clip_line_tag>();
        converter_.template unset<clip_poly_tag>();
    }

    void operator()(geometry::polygon<double> const & geo) const
    {
        converter_.template set<clip_poly_tag>();
        converter_.template unset<clip_line_tag>();
    }

    template <typename T>
    void operator()(T const&) const
    {
    }

private:
    VertexConverter & converter_;
    const value_bool clip_;
};

template <typename VertexConverter>
class set_line_clip_geometry_visitor
{
public:
    set_line_clip_geometry_visitor(
        VertexConverter & converter,
        value_bool clip)
        : converter_(converter),
          clip_(clip)
    {
    }

    void operator()(geometry::line_string<double> const & geo) const
    {
        converter_.template set<clip_line_tag>();
        converter_.template unset<clip_poly_tag>();
    }

    void operator()(geometry::polygon<double> const & geo) const
    {
        converter_.template set<clip_line_tag>();
        converter_.template unset<clip_poly_tag>();
    }

    template <typename T>
    void operator()(T const&) const
    {
    }

private:
    VertexConverter & converter_;
    const value_bool clip_;
};

template <template <class> class SetClip, typename SubLayout>
class vertex_converter : util::noncopyable
{
    template <typename Layout, typename LayoutGenerator>
    struct converter_adapter
    {
        converter_adapter(Layout & layout,
            LayoutGenerator & layout_generator)
            : layout_(layout),
              layout_generator_(layout_generator)
        {
        }

        template <typename PathT>
        void add_path(PathT & path) const
        {
            status_ = layout_.try_placement(layout_generator_, path);
        }

        bool status() const { return status_; }
        Layout & layout_;
        LayoutGenerator & layout_generator_;
        mutable bool status_ = false;
    };

    template <typename Adapter, typename VertexConverter>
    class geometry_visitor
    {
    public:
        geometry_visitor(
            VertexConverter & converter,
            Adapter const & adapter)
            : converter_(converter),
              adapter_(adapter)
        {
        }

        bool operator()(geometry::line_string<double> const & geo) const
        {
            geometry::line_string_vertex_adapter<double> va(geo);
            converter_.apply(va, adapter_);
            return adapter_.status();
        }

        bool operator()(geometry::polygon<double> const & geo) const
        {
            geometry::polygon_vertex_adapter<double> va(geo);
            converter_.apply(va, adapter_);
            return adapter_.status();
        }

        bool operator()(geometry::point<double> const & geo) const
        {
            geometry::point_vertex_adapter<double> va(geo);
            converter_.apply(va, adapter_);
            return adapter_.status();
        }

        template <typename T>
        bool operator()(T const&) const
        {
            return false;
        }

    private:
        VertexConverter & converter_;
        Adapter const & adapter_;
    };

public:
    using box_type = box2d<double>;
    using params_type = label_placement::placement_params;

    using vertex_converter_type = mapnik::vertex_converter<
        clip_line_tag,
        clip_poly_tag,
        transform_tag,
        affine_transform_tag,
        extend_tag,
        simplify_tag,
        smooth_tag,
        offset_transform_tag>;

    vertex_converter(params_type const & params)
    : sublayout_(params),
      converter_(params.query_extent, params.symbolizer,
        params.view_transform, params.proj_transform,
        params.affine_transform, params.feature, params.vars,
        params.scale_factor),
      clip_(params.get<value_bool, keys::clip>())
    {
        value_double simplify_tolerance = params.get<value_double, keys::simplify_tolerance>();
        value_double smooth = params.get<value_double, keys::smooth>();
        value_double extend = params.get<value_double, keys::extend>();
        value_double offset = params.get<value_double, keys::offset>();

        converter_.template set<transform_tag>();
        converter_.template set<affine_transform_tag>();
        if (extend > 0.0) converter_.template set<extend_tag>();
        if (simplify_tolerance > 0.0) converter_.template set<simplify_tag>();
        if (smooth > 0.0) converter_.template set<smooth_tag>();
        if (std::fabs(offset) > 0.0) converter_.template set<offset_transform_tag>();
    }

    template <typename LayoutGenerator, typename Geom>
    bool try_placement(
        LayoutGenerator & layout_generator,
        Geom & geom)
    {
        if (clip_)
        {
            using set_clip_visitor_type = SetClip<vertex_converter_type>;
            set_clip_visitor_type set_clip_visitor(converter_, clip_);
            util::apply_visitor(set_clip_visitor, geom);
        }

        using adapter_type = converter_adapter<SubLayout, LayoutGenerator>;
        using visitor_type = geometry_visitor<adapter_type, vertex_converter_type>;
        adapter_type adapter(sublayout_, layout_generator);
        visitor_type visitor(converter_, adapter);
        return util::apply_visitor(visitor, geom);
    }

private:
    SubLayout sublayout_;
    vertex_converter_type converter_;
    const value_bool clip_;
};

} }

#endif // MAPNIK_LABEL_PLACEMENT_VERTEX_CONVERTER_HPP

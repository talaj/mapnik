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

#ifndef MAPNIK_GEOMETRY_CENTROID_HPP
#define MAPNIK_GEOMETRY_CENTROID_HPP

#include <mapnik/geometry.hpp>
#include <mapnik/geometry_adapters.hpp>
#include <boost/geometry/algorithms/centroid.hpp>

namespace mapnik { namespace geometry {

namespace detail {

template <typename T>
struct geometry_centroid
{
    using result_type = bool;

    geometry_centroid(point<T> & pt)
        : pt_(pt) {}

    template <typename T1>
    result_type operator() (T1 const& geom) const
    {
        return util::apply_visitor(*this, geom);
    }

    result_type operator() (geometry_empty const&) const
    {
        return false;
    }

    result_type operator() (geometry_collection<T> const&) const
    {
        return false;
    }

    result_type operator() (point<T> const& geom) const
    {
        boost::geometry::centroid(geom, pt_);
        return true;
    }

    result_type operator() (line_string<T> const& geom) const
    {
        boost::geometry::centroid(geom, pt_);
        return true;
    }

    result_type operator() (polygon<T> const& geom) const
    {
        boost::geometry::centroid(geom, pt_);
        return true;
    }

    result_type operator() (multi_point<T> const& geom) const
    {
        boost::geometry::centroid(geom, pt_);
        return true;
    }

    result_type operator() (multi_line_string<T> const& geom) const
    {
        boost::geometry::centroid(geom, pt_);
        return true;
    }

    result_type operator() (multi_polygon<T> const& geom) const
    {
        boost::geometry::centroid(geom, pt_);
        return true;
    }
    point<T> & pt_;
};

}

template <typename T1, typename T2>
inline bool centroid(T1 const& geom, point<T2> & pt)
{
    return detail::geometry_centroid<T2>(pt)(geom);
}

}}

#endif // MAPNIK_GEOMETRY_CENTROID_HPP

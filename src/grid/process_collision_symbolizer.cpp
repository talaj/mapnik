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

#if defined(GRID_RENDERER)

#include <mapnik/grid/grid_renderer.hpp>
#include <mapnik/renderer_common/process_collision_symbolizer.hpp>

namespace mapnik {

template <typename T>
void grid_renderer<T>::process(
    collision_symbolizer const & sym,
    mapnik::feature_impl & feature,
    proj_transform const & prj_trans)
{
    process_collision_symbolizer(sym, feature, prj_trans, common_);
}

template void grid_renderer<grid>::process(
    collision_symbolizer const &,
    mapnik::feature_impl &,
    proj_transform const &);

}

#endif

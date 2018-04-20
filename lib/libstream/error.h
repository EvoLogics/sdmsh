//*************************************************************************
// This is free software: you can redistribute it and/or modify it        *
// under the terms of the GNU General Public License version 3 as         *
// published by the Free Software Foundation.                             *
//                                                                        *
// This program is distributed in the hope that it will be useful, but    *
// WITHOUT ANY WARRANTY; without even the implied warranty of FITNESS     *
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for       *
// more details.                                                          *
//                                                                        *
// You should have received a copy of the GNU General Public License      *
// along with this program. If not, see <http://www.gnu.org/licenses/>.   *
//*************************************************************************
// Author: Oleksiy Kebkal                                                 *
//*************************************************************************

#ifndef SDM_ERROR_H_INCLUDED_
#define SDM_ERROR_H_INCLUDED_

//! No error.
#define SDM_ERROR_NONE          0
//! Resource not found.
#define SDM_ERROR_NOT_FOUND    -1
#define SDM_ERROR_FILE         -2
//! Stream error.
#define SDM_ERROR_STREAM       -3
#define SDM_ERROR_OTHER        -4
//! End of stream.
#define SDM_ERROR_EOS          -5
//! Buffer overrun.
#define SDM_ERROR_OVERRUN      -6
//! Buffer underrun.
#define SDM_ERROR_UNDERRUN     -7

#endif

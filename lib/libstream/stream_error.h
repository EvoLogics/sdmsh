/**************************************************************************
 * This is free software: you can redistribute it and/or modify it        *
 * under the terms of the GNU General Public License version 3 as         *
 * published by the Free Software Foundation.                             *
 *                                                                        *
 * This program is distributed in the hope that it will be useful, but    *
 * WITHOUT ANY WARRANTY; without even the implied warranty of FITNESS     *
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for       *
 * more details.                                                          *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.   *
 **************************************************************************
 * Author: Oleksiy Kebkal                                                 *
 **************************************************************************
 */

#ifndef STREAM_ERROR_H_INCLUDED_
#define STREAM_ERROR_H_INCLUDED_

/*! No error */
#define STREAM_ERROR_NONE          0
#define STREAM_ERROR_MIN          -1001
/*! Resource not found */
#define STREAM_ERROR_NOT_FOUND    -1001
/*! Stream file operation */
#define STREAM_ERROR_IO           -1002
/*! Stream error.*/
#define STREAM_ERROR              -1003
#define STREAM_ERROR_OTHER        -1004
/*! End of stream */
#define STREAM_ERROR_EOS          -1005
/*! Buffer overrun */
#define STREAM_ERROR_OVERRUN      -1006
/*! Buffer underrun */
#define STREAM_ERROR_UNDERRUN     -1007
/*! Not implemented */
#define STREAM_ERROR_OP_NOT_SUPP  -1008

#endif

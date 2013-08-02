/**
 * Copyright 2013 Felix Schmitt
 *
 * This file is part of libSplash. 
 * 
 * libSplash is free software: you can redistribute it and/or modify 
 * it under the terms of of either the GNU General Public License or 
 * the GNU Lesser General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version. 
 * libSplash is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License and the GNU Lesser General Public License 
 * for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * and the GNU Lesser General Public License along with libSplash. 
 * If not, see <http://www.gnu.org/licenses/>. 
 */ 
 


#ifndef _DIMENSIONS_H
#define	_DIMENSIONS_H

#include <sstream>
#include <hdf5.h>

namespace DCollector
{

    /**
     * Holds 3D-hsize_t-information. Can be used for mpi-positions/-dimensions or data-dimensions
     * @author fschmitt
     */
    class Dimensions
    {
    private:
        hsize_t s[3];
    public:

        /**
         * default constructor. initializes all dimensions to 1
         */
        Dimensions(void)
        {
            set(1, 1, 1);
        }

        /**
         * constructor
         * @param x first dimension
         * @param y second dimension
         * @param z third dimension
         */
        Dimensions(hsize_t x, hsize_t y, hsize_t z)
        {
            set(x, y, z);
        }

        /**
         * Destructor
         */
        virtual ~Dimensions()
        {

        }

        hsize_t & operator[](const hsize_t t)
        {
            return s[t];
        }
        
        const hsize_t & operator[](const hsize_t t) const
        {
            return s[t];
        }
        
        Dimensions operator+(Dimensions const& other) const
        {
            return Dimensions(s[0] + other[0], s[1] + other[1], s[2] + other[2]);
        }
        
        Dimensions operator-(Dimensions const& other) const
        {
            return Dimensions(s[0] - other[0], s[1] - other[1], s[2] - other[2]);
        }
        
        Dimensions operator*(Dimensions const& other) const
        {
            return Dimensions(s[0] * other[0], s[1] * other[1], s[2] * other[2]);
        }
        
        Dimensions operator/(Dimensions const& other) const
        {
            return Dimensions(s[0] / other[0], s[1] / other[1], s[2] / other[2]);
        }
        
        bool operator==(Dimensions const& other) const
        {
            return s[0] == other[0] && s[1] == other[1] && s[2] == other[2];
        }
        
        bool operator!=(Dimensions const& other) const
        {
            return !(*this == other);
        }

        std::string toString() const
        {
            std::stringstream stream;
            stream << "(" << s[0] << "," << s[1] << "," << s[2] << ")";
            return stream.str();
        }

        /**
         * get pointer to data array
         * @return pointer to the data array
         */
        inline hsize_t *getPointer()
        {
            return s;
        }
        
        inline const hsize_t *getPointer() const
        {
            return s;
        }

        /**
         * get the size in bytes of the data array
         * @return size in bytes of data array
         */
        inline static size_t getSize()
        {
            return 3 * sizeof (hsize_t);
        }

        /**
         * get the size in elements of all dimensions
         * @return total elements in all dimensions
         */
        inline size_t getDimSize() const
        {
            return s[0] * s[1] * s[2];
        }

        /**
         * set dimensions data
         * @param x first dimension
         * @param y second dimension
         * @param z third dimension
         */
        inline void set(hsize_t x, hsize_t y, hsize_t z)
        {
            s[0] = x;
            s[1] = y;
            s[2] = z;
        }

        /**
         * set dimensions data
         * @param d Dimensions object to copy data from
         */
        inline void set(Dimensions d)
        {
            s[0] = d[0];
            s[1] = d[1];
            s[2] = d[2];
        }

        /**
         * swaps the dimensions depending on rank
         */
        void swapDims(uint32_t rank)
        {
            hsize_t tmp1 = s[0];
            hsize_t tmp2[3] = {s[2], s[1], s[0]};

            switch (rank)
            {
                case 2:
                    s[0] = s[1];
                    s[1] = tmp1;
                    break;
                case 3:
                    for (uint32_t i = 0; i < 3; i++)
                        s[i] = tmp2[i];
                    break;
                default:
                    return;
            }
        }

    };

}

#endif	/* _DIMENSIONS_H */


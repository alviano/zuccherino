/*
 *  Copyright (C) 2017  Mario Alviano (mario@alviano.net)
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef zuccherino_vec_h
#define zuccherino_vec_h

#include <mtl/Vec.h>

namespace zuccherino {
    
template<class T>
class vec : public Glucose::vec<T> {
public:
    inline vec() : Glucose::vec<T>() {}
    inline vec(const vec<T>& init) : Glucose::vec<T>() { init.copyTo(*this); }
    inline vec<T>& operator=(const vec<T>& right) { right.copyTo(*this); return *this; }
    
    void sort();

private:
    void quickSort(int left, int right);
};

template <class T>
void vec<T>::quickSort(int left, int right) {
    vec<T>& arr = *this;
    
    int i = left, j = right;
    T tmp;
    assert((left + right) / 2 >= 0);
    assert_msg((left + right) / 2 < arr.size(), "Accessing element " << (left + right) / 2 << " in array of size " << arr.size());
    int pivot = arr[(left + right) / 2];

    /* partition */
    while (i <= j) {
        while (arr[i] < pivot)
              i++;
        while (arr[j] > pivot)
              j--;
        if (i <= j) {
              tmp = arr[i];
              arr[i] = arr[j];
              arr[j] = tmp;
              i++;
              j--;
        }
    };

    /* recursion */
    if (left < j)
        quickSort(left, j);
    if (i < right)
        quickSort(i, right);
}

template <class T>
void vec<T>::sort() {
    if(this->size() > 1) quickSort(0, this->size()-1);
}

}

#endif

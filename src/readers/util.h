/*******************************************************************************
 * Copyright 2011 See AUTHORS file.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/
/** @author Xoppa */
#ifdef _MSC_VER
#pragma once
#endif //_MSC_VER
#ifndef FBXCONV_READERS_UTIL_H
#define FBXCONV_READERS_UTIL_H

#include <vector>
#include <algorithm>
#include <assert.h>

namespace fbxconv {
namespace readers {

	// Index + weight pair for vertex blending
	struct BlendWeight {
		float weight;
		int index;
		BlendWeight() : weight(0.f), index(-1) {}
		BlendWeight(float w, int i) : weight(w), index(i) {}
		inline bool operator<(const BlendWeight& rhs) const {
			return weight < rhs.weight;
		}
		inline bool operator>(const BlendWeight& rhs) const {
			return weight > rhs.weight;
		}
		inline bool operator==(const BlendWeight& rhs) const {
			return weight == rhs.weight;
		}
	};

	// Group of indices for vertex blending
	struct BlendBones {
    private:
		int *_bones;
		unsigned int _capacity;
    public:
		BlendBones(const unsigned int &capacity = 2) : _capacity(capacity) {
			_bones = new int[capacity];
			memset(_bones, -1, capacity * sizeof(int));
		}
		BlendBones(const BlendBones &rhs) : _capacity(rhs._capacity) {
			_bones = new int[_capacity];
			memcpy(_bones, rhs._bones, _capacity * sizeof(int));
		}
		~BlendBones() {
			delete _bones;
		}
		inline bool has(const int &bone) const {
			for (unsigned int i = 0; i < _capacity; i++)
				if (_bones[i] == bone)
					return true;
			return false;
		}
		inline unsigned int size() const {
			for (unsigned int i = 0; i < _capacity; i++)
				if (_bones[i] < 0)
					return i;
			return _capacity;
		}
		inline unsigned int available() const {
			return _capacity - size();
		}
		inline int cost(const std::vector<std::vector<BlendWeight>*> &rhs) const {
			int result = 0;
			for (std::vector<std::vector<BlendWeight>*>::const_iterator itr = rhs.begin(); itr != rhs.end(); ++itr)
				for (std::vector<BlendWeight>::const_iterator jtr = (*itr)->begin(); jtr != (*itr)->end(); ++jtr)
					if (!has((*jtr).index))
						result++;
			return (result > (int)available()) ? -1 : result;
		}
		inline void sort() {
			std::sort(_bones, _bones + size());
		}
		inline int idx(const int &bone) const {
			for (unsigned int i = 0; i < _capacity; i++)
				if (_bones[i] == bone)
					return i;
			return -1;
		}
		inline int add(const int &v) {
			for (unsigned int i = 0; i < _capacity; i++) {
				if (_bones[i] == v)
					return i;
				else if (_bones[i] < 0) {
					_bones[i] = v;
					return i;
				}
			}
			return -1;
		}
		inline bool add(const std::vector<std::vector<BlendWeight>*> &rhs) {
			for (std::vector<std::vector<BlendWeight>*>::const_iterator itr = rhs.begin(); itr != rhs.end(); ++itr)
				for (std::vector<BlendWeight>::const_iterator jtr = (*itr)->begin(); jtr != (*itr)->end(); ++jtr)
					if (add((*jtr).index)<0)
						return false;
			return true;
		}
		inline int operator[](const unsigned int idx) const {
			return idx < _capacity ? _bones[idx] : -1;
		}
		inline BlendBones &operator=(const BlendBones &rhs) {
			if (&rhs == this)
				return *this;
			if (_capacity != rhs._capacity) {
				delete[] _bones;
				_bones = new int[_capacity = rhs._capacity];
			}
			memcpy(_bones, rhs._bones, _capacity * sizeof(int));
			return *this;
		}
	};

	// Collection of group of indices for vertex blending
	struct BlendBonesCollection {
    private:
		std::vector<BlendBones> _bones;
		unsigned int _bonesCapacity;
    public:
		BlendBonesCollection(const unsigned int &bonesCapacity) : _bonesCapacity(bonesCapacity) { }
		BlendBonesCollection(const BlendBonesCollection &rhs) : _bonesCapacity(rhs._bonesCapacity) {
			_bones.insert(_bones.begin(), rhs._bones.begin(), rhs._bones.end());
		}
		inline BlendBonesCollection &operator=(const BlendBonesCollection &rhs) {
			if (&rhs == this)
				return (*this);
			_bones = rhs._bones;
			_bonesCapacity = rhs._bonesCapacity;
			return (*this);
		}
		inline unsigned int size() const {
			return (unsigned int)_bones.size();
		}
        const std::vector<BlendBones>& getBones() const { return _bones; }
		inline BlendBones &operator[](const unsigned int &idx) {
			return _bones[idx];
		}
		inline unsigned int add(const std::vector<std::vector<BlendWeight>*> &rhs) {
			int cost = (int)_bonesCapacity, idx = -1, n = _bones.size();
			for (int i = 0; i < n; i++) {
				const int c = _bones[i].cost(rhs);
				if (c >= 0 && c < cost) {
					cost = c;
					idx = i;
				}
			}
			if (idx < 0) {
				_bones.push_back(BlendBones(_bonesCapacity));
				idx = n;
			}
			return _bones[idx].add(rhs) ? idx : -1;
		}
		inline void sortBones() {
			for (std::vector<BlendBones>::iterator itr = _bones.begin(); itr != _bones.end(); ++itr)
				(*itr).sort();
		}
	};

	// Provides information about an animation
	struct AnimInfo {
		float start;
		float stop;
		float framerate;
		bool translate;
		bool rotate;
		bool scale;

		AnimInfo() : start(FLT_MAX), stop(-1.f), framerate(0.f), translate(false), rotate(false), scale(false) {}

		inline AnimInfo& operator+=(const AnimInfo& rhs) {
			start = std::min(rhs.start, start);
			stop = std::max(rhs.stop, stop);
			framerate = std::max(rhs.framerate, framerate);
			translate = translate || rhs.translate;
			rotate = rotate || rhs.rotate;
			scale = scale || rhs.scale;
			return *this;
		}
	};
} }
#endif //FBXCONV_READERS_UTIL_H
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
#endif
#ifndef MODELDATA_MESH_H
#define MODELDATA_MESH_H

#include <vector>
#include "MeshPart.h"
#include "Attributes.h"
#include "../json/BaseJSONWriter.h"

namespace fbxconv {
namespace modeldata {
	/** A mesh is responsable for freeing all parts and vertices it contains. */
	struct Mesh : public json::ConstSerializable {
		/** the attributes the vertices in this mesh describe */
		Attributes _attributes;
		/** the size (in number of floats) of each vertex */
		unsigned int _vertexSize;
		/** the vertices that this mesh contains */
		std::vector<float> _vertices;
		/** hash lookup table for faster duplicate vertex checking */
		std::vector<unsigned int> _hashes;
		/** the indexed parts of this mesh */
		std::vector<MeshPart *> _parts;
        std::string _name;

		/** ctor */
		Mesh() : _attributes(0), _vertexSize(0), _name("unnammed") {}

		/** copy constructor */
		Mesh(const Mesh &copyFrom) {
            _name = copyFrom._name;
			_attributes = copyFrom._attributes;
			_vertexSize = copyFrom._vertexSize;
			_vertices.insert(_vertices.end(), copyFrom._vertices.begin(), copyFrom._vertices.end());
			for (std::vector<MeshPart *>::const_iterator itr = copyFrom._parts.begin(); itr != copyFrom._parts.end(); ++itr)
				_parts.push_back(new MeshPart(**itr));
		}

		~Mesh() {
			clear();
		}

		void clear() {
			_vertices.clear();
			_hashes.clear();
			_attributes = _vertexSize = 0;
			for (std::vector<MeshPart *>::iterator itr = _parts.begin(); itr != _parts.end(); ++itr)
				delete (*itr);
			_parts.clear();
		}

		inline unsigned int indexCount() {
			unsigned int result = 0;
			for (std::vector<MeshPart *>::const_iterator itr = _parts.begin(); itr != _parts.end(); ++itr)
				result += (unsigned int)(*itr)->indices.size();
			return result;
		}

		inline unsigned int vertexCount() {
			return _vertices.size() / _vertexSize;
		}

		inline unsigned int add(const float *vertex) {
			const unsigned int hash = calcHash(vertex, _vertexSize);
			const unsigned int n = (unsigned int)_hashes.size();
			for (unsigned int i = 0; i < n; i++)
				if ((_hashes[i] == hash) && compare(&_vertices[i*_vertexSize], vertex, _vertexSize))
					return i;
			_hashes.push_back(hash);
			_vertices.insert(_vertices.end(), &vertex[0], &vertex[_vertexSize]);
			return (unsigned int)_hashes.size() - 1;
		}

		inline unsigned int calcHash(const float *vertex, const unsigned int size) {
			unsigned int result = 0;
			for (unsigned int i = 0; i < size; i++)
				result += ((*((unsigned int *)&vertex[i])) & 0xffffff00) >> 8;
			return result;
		}

		inline bool compare(const float* lhs, const float* rhs, const unsigned int &n) {
			for (unsigned int i = 0; i < n; i++)
				if ((*(unsigned int*)&lhs[i] != *(unsigned int*)&rhs[i]) && lhs[i] != rhs[i])
					return false;
			return true;
		}

		virtual void serialize(json::BaseJSONWriter &writer) const;
	};
}
}

#endif //MODELDATA_MESH_H

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
#ifndef FBXCONV_READERS_FBXMESHINFO_H
#define FBXCONV_READERS_FBXMESHINFO_H

#include <fbxsdk.h>
#include "../modeldata/Model.h"
#include <sstream>
#include <map>
#include <algorithm>
#include <functional>
#include <assert.h>
#include "util.h"
#include "matrix3.h"
#include "../log/log.h"

using namespace fbxconv::modeldata;

namespace fbxconv {
namespace readers {
	struct FbxMeshInfo {
		// The source mesh of which the values below are extracted
		const FbxMesh * const _mesh;
        // mesh name
        const std::string _meshName;
		// The ID of the mesh (shape)
		const std::string id;
		// The maximum amount of blend weights per vertex
		const unsigned int maxVertexBlendWeightCount;
		// The actual amount of blend weights per vertex (<= maxVertexBlendWeightCount)
		unsigned int vertexBlendWeightCount;
		// Whether to use maxVertexBlendWeightCount even if the actual amount of vertex weights is less than that
		const bool _forceMaxVertexBlendWeightCount;
		// Whether the required minimum amount of bones (per triangle) exceeds the specified maxNodePartBoneCount
		bool bonesOverflow;
		// The vertex attributes
		Attributes attributes;
		// Whether to use packed colors
		const bool _usePackedColors;
		// The number of polygon (triangles if triangulated)
		//const unsigned int _polyCount;
		// The number of control points within the mesh
		//const unsigned int _pointCount;
		// The control points within the mesh
		//const FbxVector4 * const _points;
		// The number of texture coordinates within the mesh
		const unsigned int uvCount;
		// The number of element materials
		//const int elementMaterialCount;
		// The number of mash parts within the mesh
		int _meshPartCount;
		// The applied skin or 0 if not available
		FbxSkin * const skin;
		// The blendweights per control point
		std::vector<BlendWeight> *_pointBlendWeights;
		// The collection of bones per mesh part
		std::vector<BlendBonesCollection> _partBones;
		// Mapping between the polygon and the index of its meshpart
		unsigned int * _polyPartMap;
		// Mapping between the polygon and the index of its weight bones within its meshpart
		unsigned int * _polyPartBonesMap;
		// The UV bounds per part per uv coords
		float * _partUVBounds;
		// The mapping name of each uv to identify the cooresponding texture
		std::string uvMapping[8];

		const FbxLayerElementArrayTemplate<FbxVector4> *normals;
		const FbxLayerElementArrayTemplate<int> *normalIndices;
		bool normalOnPoint;

		const FbxLayerElementArrayTemplate<FbxVector4> *tangents;
		const FbxLayerElementArrayTemplate<int> *tangentIndices;
		bool tangentOnPoint;

		const FbxLayerElementArrayTemplate<FbxVector4> *binormals;
		const FbxLayerElementArrayTemplate<int> *binormalIndices;
		bool binormalOnPoint;

		const FbxLayerElementArrayTemplate<FbxColor> *colors;
		const FbxLayerElementArrayTemplate<int> *colorIndices;
		bool colorOnPoint;

		const FbxLayerElementArrayTemplate<FbxVector2>*uvs[8];
		const FbxLayerElementArrayTemplate<int> *uvIndices[8];
		bool uvOnPoint[8];

		fbxconv::log::Log *log;

        FbxMeshInfo(fbxconv::log::Log *log, const std::string& meshName,FbxMesh * const &mesh, const bool &usePackedColors, const unsigned int &maxVertexBlendWeightCount, const bool &forceMaxVertexBlendWeightCount, const unsigned int &maxNodePartBoneCount)
			: _meshName(meshName), _mesh(mesh), log(log),
			_usePackedColors(usePackedColors),
			maxVertexBlendWeightCount(4),
			vertexBlendWeightCount(0),
			_forceMaxVertexBlendWeightCount(true),
			//_pointCount(mesh->GetControlPointsCount()),
			//_polyCount(mesh->GetPolygonCount()),
			//_points(mesh->GetControlPoints()),
			//elementMaterialCount(mesh->GetElementMaterialCount()),
			uvCount((unsigned int)(mesh->GetElementUVCount() > 8 ? 8 : mesh->GetElementUVCount())),
			_pointBlendWeights(0),
			skin((maxNodePartBoneCount > 0 && maxVertexBlendWeightCount > 0 && (unsigned int)mesh->GetDeformerCount(FbxDeformer::eSkin) > 0) ? static_cast<FbxSkin*>(mesh->GetDeformer(0, FbxDeformer::eSkin)) : 0),
			bonesOverflow(false),
			id(getID(mesh))
		{
            _polyPartMap = getPolyCount() > 0 ? new unsigned int[getPolyCount()] : 0;
            _polyPartBonesMap = getPolyCount() > 0 ? new unsigned int[getPolyCount()] : 0;
			
            _meshPartCount = calcMeshPartCount();
			_partBones = std::vector<BlendBonesCollection>(_meshPartCount, BlendBonesCollection(maxNodePartBoneCount));
			_partUVBounds = _meshPartCount * uvCount > 0 ? new float[4 * _meshPartCount * uvCount] : 0;
			memset(_polyPartMap, -1, sizeof(unsigned int) * getPolyCount());
			memset(_polyPartBonesMap, 0, sizeof(unsigned int) * getPolyCount());
			if (_partUVBounds)
				memset(_partUVBounds, -1, sizeof(float) * 4 * _meshPartCount * uvCount);

			if (skin) {
				fetchVertexBlendWeights();
				fetchMeshPartsAndBones();
			}
			else
				fetchMeshParts();

			fetchAttributes();
			cacheAttributes();
			fetchUVInfo();
		}

		~FbxMeshInfo() {
			if (_pointBlendWeights)
				delete[] _pointBlendWeights;
			if (_polyPartMap)
				delete[] _polyPartMap;
			if (_polyPartBonesMap)
				delete[] _polyPartBonesMap;
			if (_partUVBounds)
				delete[] _partUVBounds;
		}
        
        inline unsigned int getPolyCount() const {
            return _mesh->GetPolygonCount();
        }
        
		inline FbxCluster *getBone(const unsigned int &idx) {
			return skin ? skin->GetCluster(idx) : 0;
		}

		inline void getPosition(float * const &data, unsigned int &offset, const unsigned int &point) const {
            auto points = _mesh->GetControlPoints();
			const FbxVector4 &position = points[point];
			data[offset++] = (float)position[0];
			data[offset++] = (float)position[1];
			data[offset++] = (float)position[2];
		}

		inline void getNormal(FbxVector4 * const &out, const unsigned int &polyIndex, const unsigned int &point) const {
			((FbxLayerElementArray*)normals)->GetAt<FbxVector4>(normalOnPoint ? (normalIndices ? (*normalIndices)[point] : point) : (normalIndices ? (*normalIndices)[polyIndex]: polyIndex), out);
			//return normalOnPoint ? (*normals)[normalIndices ? (*normalIndices)[point] : point] : (*normals)[normalIndices ? (*normalIndices)[polyIndex]: polyIndex];
		}

		inline void getNormal(float * const &data, unsigned int &offset, const unsigned int &polyIndex, const unsigned int &point) const {
			static FbxVector4 tmpV4;
			getNormal(&tmpV4, polyIndex, point);
			data[offset++] = (float)tmpV4.mData[0];
			data[offset++] = (float)tmpV4.mData[1];
			data[offset++] = (float)tmpV4.mData[2];
		}

		inline void getTangent(FbxVector4 * const &out, const unsigned int &polyIndex, const unsigned int &point) const {
			((FbxLayerElementArray*)tangents)->GetAt<FbxVector4>(tangentOnPoint ? (tangentIndices ? (*tangentIndices)[point] : point) : (tangentIndices ? (*tangentIndices)[polyIndex] : polyIndex), out);
			//return tangentOnPoint ? (*tangents)[tangentIndices ? (*tangentIndices)[point] : point] : (*tangents)[tangentIndices ? (*tangentIndices)[polyIndex] : polyIndex];
		}

		inline void getTangent(float * const &data, unsigned int &offset, const unsigned int &polyIndex, const unsigned int &point) const {
			static FbxVector4 tmpV4;
			getTangent(&tmpV4, polyIndex, point);
			data[offset++] = (float)tmpV4.mData[0];
			data[offset++] = (float)tmpV4.mData[1];
			data[offset++] = (float)tmpV4.mData[2];
		}

		inline void getBinormal(FbxVector4* const &out, const unsigned int &polyIndex, const unsigned int &point) const {
			((FbxLayerElementArray*)binormals)->GetAt<FbxVector4>(binormalOnPoint ? (binormalIndices ? (*binormalIndices)[point] : point) : (binormalIndices ? (*binormalIndices)[polyIndex] : polyIndex), out);
			//return binormalOnPoint ? (*binormals)[binormalIndices ? (*binormalIndices)[point] : point] : (*binormals)[binormalIndices ? (*binormalIndices)[polyIndex] : polyIndex];
		}

		inline void getBinormal(float * const &data, unsigned int &offset, const unsigned int &polyIndex, const unsigned int &point) const {
			static FbxVector4 tmpV4;
			getBinormal(&tmpV4, polyIndex, point);
			data[offset++] = (float)tmpV4.mData[0];
			data[offset++] = (float)tmpV4.mData[1];
			data[offset++] = (float)tmpV4.mData[2];
		}

		inline void getColor(FbxColor * const &out, const unsigned int &polyIndex, const unsigned int &point) const {
			((FbxLayerElementArray*)colors)->GetAt<FbxColor>(colorOnPoint ? (colorIndices ? (*colorIndices)[point] : point) : (colorIndices ? (*colorIndices)[polyIndex] : polyIndex), out);
			//return colorOnPoint ? (*colors)[colorIndices ? (*colorIndices)[point] : point] : (*colors)[colorIndices ? (*colorIndices)[polyIndex] : polyIndex];
		}

		inline void getColor(float * const &data, unsigned int &offset, const unsigned int &polyIndex, const unsigned int &point) const {
			static FbxColor tmpC;
			getColor(&tmpC, polyIndex, point);
			data[offset++] = (float)tmpC.mRed;
			data[offset++] = (float)tmpC.mGreen;
			data[offset++] = (float)tmpC.mBlue;
			data[offset++] = (float)tmpC.mAlpha;
		}

		inline void getColorPacked(float * const &data, unsigned int &offset, const unsigned int &polyIndex, const unsigned int &point) const {
			static FbxColor tmpC;
			getColor(&tmpC, polyIndex, point);
			unsigned int packedColor = ((unsigned int)(255.*tmpC.mAlpha)<<24) | ((unsigned int)(255.*tmpC.mBlue)<<16) | ((unsigned int)(255.*tmpC.mGreen)<<8) | ((unsigned int)(255.*tmpC.mRed));
			data[offset++] = *(float*)&packedColor;
		}

		inline void getUV(FbxVector2 * const &out, const unsigned int &uvIndex, const unsigned int &polyIndex, const unsigned int &point) const {
			((FbxLayerElementArray*)uvs[uvIndex])->GetAt(uvOnPoint[uvIndex] ? (uvIndices[uvIndex] ? (*uvIndices[uvIndex])[point] : point) : (uvIndices[uvIndex] ? (*uvIndices[uvIndex])[polyIndex] : polyIndex), out);
			//return uvOnPoint[uvIndex] ? (*uvs[uvIndex]).GetAt(uvIndices[uvIndex] ? (*uvIndices[uvIndex])[point] : point) : (*uvs[uvIndex]).GetAt(uvIndices[uvIndex] ? (*uvIndices[uvIndex])[polyIndex] : polyIndex);
		}

		inline void getUV(float * const &data, unsigned int &offset, const unsigned int &uvIndex, const unsigned int &polyIndex, const unsigned int &point, const Matrix3<float> &transform) const {
			static FbxVector2 uv;
			getUV(&uv, uvIndex, polyIndex, point);
			data[offset++] = (float)uv.mData[0];
			data[offset++] = (float)uv.mData[1];
			transform.transform(data[offset-2], data[offset-1]);
		}

		inline void getBlendInfos(float * const &data, unsigned int &offset, const unsigned int &poly, const unsigned int &polyIndex, const unsigned int &point) const {
            for(int weightIndex = 0; weightIndex < 4; ++weightIndex) {
                const std::vector<BlendWeight> &weights = _pointBlendWeights[point];
                const unsigned int s = (unsigned int)weights.size();
                const BlendBones &bones = _partBones[_polyPartMap[poly]].getBones()[_polyPartBonesMap[poly]];
                data[offset+weightIndex] = weightIndex < s ? (float)bones.idx(weights[weightIndex].index) : 0.f;
                data[offset+weightIndex + 4] = weightIndex < s ? weights[weightIndex].weight : 0.f;
            }
            
            offset += 2 * 4;
			
		}

		inline void getVertex(float * const &data, unsigned int &offset, const unsigned int &poly, const unsigned int &polyIndex, const unsigned int &point, const Matrix3<float> * const &uvTransforms) const {
			if (attributes.hasPosition())
				getPosition(data, offset, point);
			if (attributes.hasNormal())
				getNormal(data, offset, polyIndex, point);
			if (attributes.hasColor())
				getColor(data, offset, polyIndex, point);
			if (attributes.hasColorPacked())
				getColorPacked(data, offset, polyIndex, point);
			if (attributes.hasTangent())
				getTangent(data, offset, polyIndex, point);
			if (attributes.hasBinormal())
				getBinormal(data, offset, polyIndex, point);
			for (unsigned int i = 0; i < uvCount; i++)
				getUV(data, offset, i, polyIndex, point, uvTransforms[i]);
            if(attributes.hasBlendInfo())
                getBlendInfos(data, offset, poly, polyIndex, point);
		}

		inline void getVertex(float * const &data, const unsigned int &poly, const unsigned int &polyIndex, const unsigned int &point, const Matrix3<float> * const &uvTransforms) const {
			unsigned int offset = 0;
			getVertex(data, offset, poly, polyIndex, point, uvTransforms);
		}
	private:
		static std::string getID(FbxMesh * const &mesh) {
			static int idCounter = 0;
			const char *name = mesh->GetName();
			std::stringstream ss;
			if (name != 0 && strlen(name) > 1)
				ss << name;
			else
				ss << "shape" << (++idCounter);
			return ss.str();
		}
		
		unsigned int calcMeshPartCount() {
			int mp, mpc = 0;
			for (unsigned int poly = 0; poly < getPolyCount(); poly++) {
				mp = -1;
				for (int i = 0; i < _mesh->GetElementMaterialCount() && mp < 0; i++)
					mp = _mesh->GetElementMaterial(i)->GetIndexArray()[poly];
				if (mp >= mpc)
					mpc = mp+1;
			}
			if (mpc == 0)
				mpc = 1;
			return mpc;
		}

		void fetchAttributes() {
			attributes.hasPosition(true);
			attributes.hasNormal(_mesh->GetElementNormalCount() > 0);
			attributes.hasColor((!_usePackedColors) && (_mesh->GetElementVertexColorCount() > 0));
			attributes.hasColorPacked(_usePackedColors && (_mesh->GetElementVertexColorCount() > 0));
			attributes.hasTangent(_mesh->GetElementTangentCount() > 0);
			attributes.hasBinormal(_mesh->GetElementBinormalCount() > 0);
			for (unsigned int i = 0; i < 8; i++)
				attributes.hasUV(i, i < uvCount);
            attributes.hasBlendInfo(vertexBlendWeightCount > 0);
		}

		void cacheAttributes() {
			// Cache normals, whether they are indexed and if they are located on control points or polygon points.
			normals = attributes.hasNormal() ? &(_mesh->GetElementNormal()->GetDirectArray()) : 0;
			normalIndices = attributes.hasNormal() && _mesh->GetElementNormal()->GetReferenceMode() == FbxGeometryElement::eIndexToDirect ? &(_mesh->GetElementNormal()->GetIndexArray()) : 0;
			normalOnPoint = attributes.hasNormal() ? _mesh->GetElementNormal()->GetMappingMode() == FbxGeometryElement::eByControlPoint : false;

			// Cache tangents, whether they are indexed and if they are located on control points or polygon points.
			tangents = attributes.hasTangent() ? &(_mesh->GetElementTangent()->GetDirectArray()) : 0;
			tangentIndices = attributes.hasTangent() && _mesh->GetElementTangent()->GetReferenceMode() == FbxGeometryElement::eIndexToDirect ? &(_mesh->GetElementTangent()->GetIndexArray()) : 0;
			tangentOnPoint = attributes.hasTangent() ? _mesh->GetElementTangent()->GetMappingMode() == FbxGeometryElement::eByControlPoint : false;
			
			// Cache binormals, whether they are indexed and if they are located on control points or polygon points.
			binormals = attributes.hasBinormal() ? &(_mesh->GetElementBinormal()->GetDirectArray()) : 0;
			binormalIndices = attributes.hasBinormal() && _mesh->GetElementBinormal()->GetReferenceMode() == FbxGeometryElement::eIndexToDirect ? &(_mesh->GetElementBinormal()->GetIndexArray()) : 0;
			binormalOnPoint = attributes.hasBinormal() ? _mesh->GetElementBinormal()->GetMappingMode() == FbxGeometryElement::eByControlPoint : false;

			// Cache colors, whether they are indexed and if they are located on control points or polygon points.
			colors = (attributes.hasColor() || attributes.hasColorPacked()) ? &(_mesh->GetElementVertexColor()->GetDirectArray()) : 0;
			colorIndices = (attributes.hasColor() || attributes.hasColorPacked()) && _mesh->GetElementVertexColor()->GetReferenceMode() == FbxGeometryElement::eIndexToDirect ?
					&(_mesh->GetElementVertexColor()->GetIndexArray()) : 0;
			colorOnPoint = (attributes.hasColor() || attributes.hasColorPacked()) ? _mesh->GetElementVertexColor()->GetMappingMode() == FbxGeometryElement::eByControlPoint : false;

			// Cache uvs, whether they are indexed and if they are located on control points or polygon points.
			for (unsigned int i = 0; i < uvCount; i++) {
				uvs[i] = &(_mesh->GetElementUV(i)->GetDirectArray());
				uvIndices[i] = _mesh->GetElementUV(i)->GetReferenceMode() == FbxGeometryElement::eIndexToDirect ? &(_mesh->GetElementUV(i)->GetIndexArray()) : 0;
				uvOnPoint[i] = _mesh->GetElementUV(i)->GetMappingMode() == FbxGeometryElement::eByControlPoint;
			}
		}

		void fetchVertexBlendWeights() {
			_pointBlendWeights = new std::vector<BlendWeight>[_mesh->GetControlPointsCount()];
			const int &clusterCount = skin->GetClusterCount();
			// Fetch the blend weights per control point
			for (int i = 0; i < clusterCount; i++) {
				const FbxCluster * const &cluster = skin->GetCluster(i);
				const int &indexCount = cluster->GetControlPointIndicesCount();
				const int * const &clusterIndices = cluster->GetControlPointIndices();
				const double * const &clusterWeights = cluster->GetControlPointWeights();
				for (int j = 0; j < indexCount; j++) {
					if (clusterIndices[j] < 0 || clusterIndices[j] >= (int)_mesh->GetControlPointsCount() || clusterWeights[j] == 0.0)
						continue;
					_pointBlendWeights[clusterIndices[j]].push_back(BlendWeight((float)clusterWeights[j], i));
				}
			}
			// Sort the weights, so the most significant weights are first, remove unneeded weights and normalize the remaining
			bool error = false;
			for (unsigned int i = 0; i < _mesh->GetControlPointsCount(); i++) {
				std::sort(_pointBlendWeights[i].begin(), _pointBlendWeights[i].end(), std::greater<BlendWeight>());
				if (_pointBlendWeights[i].size() > maxVertexBlendWeightCount)
					_pointBlendWeights[i].resize(maxVertexBlendWeightCount);
				float len = 0.f;
				for (std::vector<BlendWeight>::const_iterator itr = _pointBlendWeights[i].begin(); itr != _pointBlendWeights[i].end(); ++itr)
					len += (*itr).weight;
				if (len == 0.f)
					error = true;
				else
					for (std::vector<BlendWeight>::iterator itr = _pointBlendWeights[i].begin(); itr != _pointBlendWeights[i].end(); ++itr)
						(*itr).weight /= len;
				if (_pointBlendWeights[i].size() > vertexBlendWeightCount)
					vertexBlendWeightCount = (unsigned int)_pointBlendWeights[i].size();
			}
			if (vertexBlendWeightCount > 0 && _forceMaxVertexBlendWeightCount)
				vertexBlendWeightCount = maxVertexBlendWeightCount;
			if (error)
				log->warning(log::wSourceConvertFbxZeroWeights);
		}

		void fetchMeshPartsAndBones() {
			std::vector<std::vector<BlendWeight>*> polyWeights;
			for (unsigned int poly = 0; poly < getPolyCount(); poly++) {
				int mp = -1;
				for (int i = 0; i < _mesh->GetElementMaterialCount() && mp < 0; i++)
					mp = _mesh->GetElementMaterial(i)->GetIndexArray()[poly];
				if (mp < 0 || mp >= _meshPartCount) {
					_polyPartMap[poly] = -1;
					log->warning(log::wSourceConvertFbxNoPolyPart, _mesh->GetName(), poly);
				}
				else {
					_polyPartMap[poly] = mp;
					const unsigned int polySize = _mesh->GetPolygonSize(poly);
					polyWeights.clear();
					for (unsigned int i = 0; i < polySize; i++)
						polyWeights.push_back(&_pointBlendWeights[_mesh->GetPolygonVertex(poly, i)]);
					const int sp = _partBones[mp].add(polyWeights);
					_polyPartBonesMap[poly] = sp < 0 ? 0 : (unsigned int)sp;
					if (sp < 0)
						bonesOverflow = true;
				}
			}
		}

		void fetchMeshParts() {
			int mp;
			for (unsigned int poly = 0; poly < getPolyCount(); poly++) {
				mp = -1;
				for (int i = 0; i < _mesh->GetElementMaterialCount() && mp < 0; i++)
					mp = _mesh->GetElementMaterial(i)->GetIndexArray()[poly];
				if (mp < 0 || mp >= _meshPartCount) {
					_polyPartMap[poly] = -1;
					log->warning(log::wSourceConvertFbxNoPolyPart, _mesh->GetName(), poly);
				}
				else
					_polyPartMap[poly] = mp;
			}
		}

		void fetchUVInfo() {
			FbxStringList uvSetNames;
			_mesh->GetUVSetNames(uvSetNames);
			for (unsigned int i = 0; i < uvCount; i++)
				uvMapping[i] = uvSetNames.GetItemAt(i)->mString.Buffer();

			if (_partUVBounds == 0 || uvCount == 0)
				return;
			FbxVector2 uv;
			int mp;
			unsigned int idx, pidx = 0, v = 0;
			for (unsigned int poly = 0; poly < getPolyCount(); poly++) {
				mp = _polyPartMap[poly];

				const unsigned int polySize = _mesh->GetPolygonSize(poly);
				for (unsigned int i = 0; i < polySize; i++) {
					v = _mesh->GetPolygonVertex(poly, i);
					if (mp >= 0) {
						for (unsigned int j = 0; j < uvCount; j++) {
							getUV(&uv, j, pidx, v);
							idx = 4 * (mp * uvCount + j);
							if (*(int*)&_partUVBounds[idx]==-1 || uv.mData[0] < _partUVBounds[idx])
								_partUVBounds[idx] = (float)uv.mData[0];
							if (*(int*)&_partUVBounds[idx+1]==-1 || uv.mData[1] < _partUVBounds[idx+1])
								_partUVBounds[idx+1] = (float)uv.mData[1];
							if (*(int*)&_partUVBounds[idx+2]==-1 || uv.mData[0] > _partUVBounds[idx+2])
								_partUVBounds[idx+2] = (float)uv.mData[0];
							if (*(int*)&_partUVBounds[idx+3]==-1 || uv.mData[1] > _partUVBounds[idx+3])
								_partUVBounds[idx+3] = (float)uv.mData[1];
						}
					}
					pidx++;
				}
			}
		}
	};
} }
#endif //FBXCONV_READERS_FBXMESHINFO_H
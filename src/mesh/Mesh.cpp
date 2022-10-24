#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <boost/container/flat_map.hpp>
#include <functional>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include "Edge.hpp"
#include "Mesh.hpp"
#include "Tetrahedron.hpp"
#include "Triangle.hpp"
#include "logging/LogMacros.hpp"
#include "math/geometry.hpp"
#include "mesh/Data.hpp"
#include "precice/types.hpp"
#include "query/Index.hpp"
#include "utils/EigenHelperFunctions.hpp"

namespace precice::mesh {

Mesh::Mesh(
    std::string name,
    int         dimensions,
    MeshID      id)
    : _name(std::move(name)),
      _dimensions(dimensions),
      _id(id),
      _boundingBox(dimensions),
      _index(*this)
{
  PRECICE_ASSERT((_dimensions == 2) || (_dimensions == 3), _dimensions);
  PRECICE_ASSERT(_name != std::string(""));
}

Mesh::VertexContainer &Mesh::vertices()
{
  return _vertices;
}

const Mesh::VertexContainer &Mesh::vertices() const
{
  return _vertices;
}

Mesh::EdgeContainer &Mesh::edges()
{
  return _edges;
}

const Mesh::EdgeContainer &Mesh::edges() const
{
  return _edges;
}

Mesh::TriangleContainer &Mesh::triangles()
{
  return _triangles;
}

const Mesh::TriangleContainer &Mesh::triangles() const
{
  return _triangles;
}

const Mesh::TetraContainer &Mesh::tetrahedra() const
{
  return _tetrahedra;
}

Mesh::TetraContainer &Mesh::tetrahedra()
{
  return _tetrahedra;
}

int Mesh::getDimensions() const
{
  return _dimensions;
}

Vertex &Mesh::createVertex(const Eigen::VectorXd &coords)
{
  PRECICE_ASSERT(coords.size() == _dimensions, coords.size(), _dimensions);
  auto nextID = _vertices.size();
  _vertices.emplace_back(coords, nextID);
  return _vertices.back();
}

Edge &Mesh::createEdge(
    Vertex &vertexOne,
    Vertex &vertexTwo)
{
  auto nextID = _edges.size();
  _edges.emplace_back(vertexOne, vertexTwo, nextID);
  return _edges.back();
}

Edge &Mesh::createUniqueEdge(
    Vertex &vertexOne,
    Vertex &vertexTwo)
{
  const std::array<VertexID, 2> vids{vertexOne.getID(), vertexTwo.getID()};
  const auto                    eend = edges().end();
  auto                          pos  = std::find_if(edges().begin(), eend,
                          [&vids](const Edge &e) -> bool {
                            const std::array<VertexID, 2> eids{e.vertex(0).getID(), e.vertex(1).getID()};
                            return std::is_permutation(vids.begin(), vids.end(), eids.begin());
                          });
  if (pos != eend) {
    return *pos;
  } else {
    return createEdge(vertexOne, vertexTwo);
  }
}

Triangle &Mesh::createTriangle(
    Edge &edgeOne,
    Edge &edgeTwo,
    Edge &edgeThree)
{
  PRECICE_ASSERT(
      edgeOne.connectedTo(edgeTwo) &&
      edgeTwo.connectedTo(edgeThree) &&
      edgeThree.connectedTo(edgeOne));
  auto nextID = _triangles.size();
  _triangles.emplace_back(edgeOne, edgeTwo, edgeThree, nextID);
  return _triangles.back();
}

Triangle &Mesh::createTriangle(
    Vertex &vertexOne,
    Vertex &vertexTwo,
    Vertex &vertexThree)
{
  auto nextID = _triangles.size();
  _triangles.emplace_back(vertexOne, vertexTwo, vertexThree, nextID);
  return _triangles.back();
}

Tetrahedron &Mesh::createTetrahedron(
    Vertex &vertexOne,
    Vertex &vertexTwo,
    Vertex &vertexThree,
    Vertex &vertexFour)
{

  auto nextID = _tetrahedra.size();
  _tetrahedra.emplace_back(vertexOne, vertexTwo, vertexThree, vertexFour, nextID);
  return _tetrahedra.back();
}

PtrData &Mesh::createData(
    const std::string &name,
    int                dimension,
    DataID             id)
{
  PRECICE_TRACE(name, dimension);
  for (const PtrData &data : _data) {
    PRECICE_CHECK(data->getName() != name,
                  "Data \"{}\" cannot be created twice for mesh \"{}\". "
                  "Please rename or remove one of the use-data tags with name \"{}\".",
                  name, _name, name);
  }
  //#rows = dimensions of current mesh #columns = dimensions of corresponding data set
  PtrData data(new Data(name, id, dimension, _dimensions));
  _data.push_back(data);
  return _data.back();
}

const Mesh::DataContainer &Mesh::data() const
{
  return _data;
}

bool Mesh::hasDataID(DataID dataID) const
{
  auto iter = std::find_if(_data.begin(), _data.end(), [dataID](const auto &dptr) {
    return dptr->getID() == dataID;
  });
  return iter != _data.end(); // if id was not found in mesh, iter == _data.end()
}

const PtrData &Mesh::data(DataID dataID) const
{
  auto iter = std::find_if(_data.begin(), _data.end(), [dataID](const auto &dptr) {
    return dptr->getID() == dataID;
  });
  PRECICE_ASSERT(iter != _data.end(), "Data with id not found in mesh.", dataID, _name);
  return *iter;
}

bool Mesh::hasDataName(const std::string &dataName) const
{
  auto iter = std::find_if(_data.begin(), _data.end(), [&dataName](const auto &dptr) {
    return dptr->getName() == dataName;
  });
  return iter != _data.end(); // if name was not found in mesh, iter == _data.end()
}

const PtrData &Mesh::data(const std::string &dataName) const
{
  auto iter = std::find_if(_data.begin(), _data.end(), [&dataName](const auto &dptr) {
    return dptr->getName() == dataName;
  });
  PRECICE_ASSERT(iter != _data.end(), "Data not found in mesh", dataName, _name);
  return *iter;
}

const std::string &Mesh::getName() const
{
  return _name;
}

MeshID Mesh::getID() const
{
  return _id;
}

bool Mesh::isValidVertexID(VertexID vertexID) const
{
  return (0 <= vertexID) && (static_cast<size_t>(vertexID) < vertices().size());
}

bool Mesh::isValidEdgeID(EdgeID edgeID) const
{
  return (0 <= edgeID) && (static_cast<size_t>(edgeID) < edges().size());
}

void Mesh::allocateDataValues()
{
  PRECICE_TRACE(_vertices.size());
  const auto expectedCount = _vertices.size();
  using SizeType           = std::remove_cv<decltype(expectedCount)>::type;
  for (PtrData &data : _data) {

    // Allocate data values
    const SizeType expectedSize = expectedCount * data->getDimensions();
    const auto     actualSize   = static_cast<SizeType>(data->values().size());
    // Shrink Buffer
    if (expectedSize < actualSize) {
      data->values().resize(expectedSize);
    }
    // Enlarge Buffer
    if (expectedSize > actualSize) {
      const auto leftToAllocate = expectedSize - actualSize;
      utils::append(data->values(), Eigen::VectorXd(Eigen::VectorXd::Zero(leftToAllocate)));
    }
    PRECICE_DEBUG("Data {} now has {} values", data->getName(), data->values().size());

    // Allocate gradient data values
    if (data->hasGradient()) {
      const SizeType spaceDimensions = data->getSpatialDimensions();

      const SizeType expectedColumnSize = expectedCount * data->getDimensions();
      const auto     actualColumnSize   = static_cast<SizeType>(data->gradientValues().cols());

      // Shrink Buffer
      if (expectedColumnSize < actualColumnSize) {
        data->gradientValues().resize(spaceDimensions, expectedColumnSize);
      }

      // Enlarge Buffer
      if (expectedColumnSize > actualColumnSize) {
        const auto columnLeftToAllocate = expectedColumnSize - actualColumnSize;
        utils::append(data->gradientValues(), Eigen::MatrixXd(Eigen::MatrixXd::Zero(spaceDimensions, columnLeftToAllocate)));
      }
      PRECICE_DEBUG("Gradient Data {} now has {} x {} values", data->getName(), data->gradientValues().rows(), data->gradientValues().cols());
    }
  }
}

void Mesh::computeBoundingBox()
{
  PRECICE_TRACE(_name);

  // Keep the bounding box if set via the API function.
  BoundingBox bb = _boundingBox.empty() ? BoundingBox(_dimensions) : BoundingBox(_boundingBox);

  for (const Vertex &vertex : _vertices) {
    bb.expandBy(vertex);
  }
  _boundingBox = std::move(bb);
  PRECICE_DEBUG("Bounding Box, {}", _boundingBox);
}

void Mesh::clear()
{
  _triangles.clear();
  _edges.clear();
  _vertices.clear();
  _tetrahedra.clear();
  _index.clear();

  for (mesh::PtrData &data : _data) {
    data->values().resize(0);
  }
}

/// @todo this should be handled by the Partition
void Mesh::clearPartitioning()
{
  _connectedRanks.clear();
  _communicationMap.clear();
  _vertexDistribution.clear();
  _vertexOffsets.clear();
  _globalNumberOfVertices = 0;
}

Mesh::VertexDistribution &Mesh::getVertexDistribution()
{
  return _vertexDistribution;
}

const Mesh::VertexDistribution &Mesh::getVertexDistribution() const
{
  return _vertexDistribution;
}

std::vector<int> &Mesh::getVertexOffsets()
{
  return _vertexOffsets;
}

const std::vector<int> &Mesh::getVertexOffsets() const
{
  return _vertexOffsets;
}

void Mesh::setVertexOffsets(std::vector<int> &vertexOffsets)
{
  _vertexOffsets = vertexOffsets;
}

int Mesh::getGlobalNumberOfVertices() const
{
  return _globalNumberOfVertices;
}

void Mesh::setGlobalNumberOfVertices(int num)
{
  _globalNumberOfVertices = num;
}

Eigen::VectorXd Mesh::getOwnedVertexData(DataID dataID)
{

  std::vector<double> ownedDataVector;
  int                 valueDim = data(dataID)->getDimensions();
  int                 index    = 0;

  for (const auto &vertex : vertices()) {
    if (vertex.isOwner()) {
      for (int dim = 0; dim < valueDim; ++dim) {
        ownedDataVector.push_back(data(dataID)->values()[index * valueDim + dim]);
      }
    }
    ++index;
  }
  Eigen::Map<Eigen::VectorXd> ownedData(ownedDataVector.data(), ownedDataVector.size());

  return ownedData;
}

void Mesh::tagAll()
{
  for (auto &vertex : _vertices) {
    vertex.tag();
  }
}

void Mesh::addMesh(
    Mesh &deltaMesh)
{
  PRECICE_TRACE();
  PRECICE_ASSERT(_dimensions == deltaMesh.getDimensions());

  boost::container::flat_map<VertexID, Vertex *> vertexMap;
  vertexMap.reserve(deltaMesh.vertices().size());
  Eigen::VectorXd coords(_dimensions);
  for (const Vertex &vertex : deltaMesh.vertices()) {
    coords    = vertex.getCoords();
    Vertex &v = createVertex(coords);
    v.setGlobalIndex(vertex.getGlobalIndex());
    if (vertex.isTagged())
      v.tag();
    v.setOwner(vertex.isOwner());
    PRECICE_ASSERT(vertex.getID() >= 0, vertex.getID());
    vertexMap[vertex.getID()] = &v;
  }

  // you cannot just take the vertices from the edge and add them,
  // since you need the vertices from the new mesh
  // (which may differ in IDs)
  for (const Edge &edge : deltaMesh.edges()) {
    VertexID vertexIndex1 = edge.vertex(0).getID();
    VertexID vertexIndex2 = edge.vertex(1).getID();
    PRECICE_ASSERT((vertexMap.count(vertexIndex1) == 1) &&
                   (vertexMap.count(vertexIndex2) == 1));
    createEdge(*vertexMap[vertexIndex1], *vertexMap[vertexIndex2]);
  }

  for (const Triangle &triangle : deltaMesh.triangles()) {
    VertexID vertexIndex1 = triangle.vertex(0).getID();
    VertexID vertexIndex2 = triangle.vertex(1).getID();
    VertexID vertexIndex3 = triangle.vertex(2).getID();
    PRECICE_ASSERT((vertexMap.count(vertexIndex1) == 1) &&
                   (vertexMap.count(vertexIndex2) == 1) &&
                   (vertexMap.count(vertexIndex3) == 1));
    createTriangle(*vertexMap[vertexIndex1], *vertexMap[vertexIndex2], *vertexMap[vertexIndex3]);
  }

  for (const Tetrahedron &tetra : deltaMesh.tetrahedra()) {
    VertexID vertexIndex1 = tetra.vertex(0).getID();
    VertexID vertexIndex2 = tetra.vertex(1).getID();
    VertexID vertexIndex3 = tetra.vertex(2).getID();
    VertexID vertexIndex4 = tetra.vertex(3).getID();

    PRECICE_ASSERT((vertexMap.count(vertexIndex1) == 1) &&
                   (vertexMap.count(vertexIndex2) == 1) &&
                   (vertexMap.count(vertexIndex3) == 1) &&
                   (vertexMap.count(vertexIndex4) == 1));
    createTetrahedron(*vertexMap[vertexIndex1], *vertexMap[vertexIndex2], *vertexMap[vertexIndex3], *vertexMap[vertexIndex4]);
  }
  _index.clear();
}

const BoundingBox &Mesh::getBoundingBox() const
{
  return _boundingBox;
}

void Mesh::expandBoundingBox(const BoundingBox &boundingBox)
{
  _boundingBox.expandBy(boundingBox);
}

bool Mesh::operator==(const Mesh &other) const
{
  bool equal = true;
  equal &= _vertices.size() == other._vertices.size() &&
           std::is_permutation(_vertices.begin(), _vertices.end(), other._vertices.begin());
  equal &= _edges.size() == other._edges.size() &&
           std::is_permutation(_edges.begin(), _edges.end(), other._edges.begin());
  equal &= _triangles.size() == other._triangles.size() &&
           std::is_permutation(_triangles.begin(), _triangles.end(), other._triangles.begin());
  return equal;
}

bool Mesh::operator!=(const Mesh &other) const
{
  return !(*this == other);
}

std::ostream &operator<<(std::ostream &os, const Mesh &m)
{
  os << "Mesh \"" << m.getName() << "\", dimensionality = " << m.getDimensions() << ":\n";
  os << "GEOMETRYCOLLECTION(\n";
  const auto  token = ", ";
  const auto *sep   = "";
  for (auto &vertex : m.vertices()) {
    os << sep << vertex;
    sep = token;
  }
  sep = ",\n";
  for (auto &edge : m.edges()) {
    os << sep << edge;
    sep = token;
  }
  sep = ",\n";
  for (auto &triangle : m.triangles()) {
    os << sep << triangle;
    sep = token;
  }
  os << "\n)";
  return os;
}

} // namespace precice::mesh

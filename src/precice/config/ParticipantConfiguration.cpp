#include "ParticipantConfiguration.hpp"
#include <algorithm>
#include <list>
#include <memory>
#include <stdexcept>
#include <utility>

#include "action/Action.hpp"
#include "action/config/ActionConfiguration.hpp"
#include "com/MPIDirectCommunication.hpp"
#include "com/SharedPointer.hpp"
#include "com/config/CommunicationConfiguration.hpp"
#include "io/ExportCSV.hpp"
#include "io/ExportContext.hpp"
#include "io/ExportVTK.hpp"
#include "io/ExportVTP.hpp"
#include "io/ExportVTU.hpp"
#include "io/SharedPointer.hpp"
#include "io/config/ExportConfiguration.hpp"
#include "logging/LogMacros.hpp"
#include "mapping/Mapping.hpp"
#include "mesh/Data.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/config/MeshConfiguration.hpp"
#include "partition/ReceivedPartition.hpp"
#include "precice/impl/MappingContext.hpp"
#include "precice/impl/MeshContext.hpp"
#include "precice/impl/Participant.hpp"
#include "precice/impl/WatchIntegral.hpp"
#include "precice/impl/WatchPoint.hpp"
#include "utils/IntraComm.hpp"
#include "utils/PointerVector.hpp"
#include "utils/assertion.hpp"
#include "utils/networking.hpp"
#include "xml/ConfigParser.hpp"
#include "xml/XMLAttribute.hpp"

namespace precice::config {

ParticipantConfiguration::ParticipantConfiguration(
    xml::XMLTag &              parent,
    mesh::PtrMeshConfiguration meshConfiguration)
    : _meshConfig(std::move(meshConfiguration))
{
  PRECICE_ASSERT(_meshConfig);
  using namespace xml;
  std::string doc;
  XMLTag      tag(*this, TAG, XMLTag::OCCUR_ONCE_OR_MORE);
  doc = "Represents one solver using preCICE. At least two ";
  doc += "participants have to be defined.";
  tag.setDocumentation(doc);

  auto attrName = XMLAttribute<std::string>(ATTR_NAME)
                      .setDocumentation(
                          "Name of the participant. Has to match the name given on construction "
                          "of the precice::SolverInterface object used by the participant.");
  tag.addAttribute(attrName);

  XMLTag tagWriteData(*this, TAG_WRITE, XMLTag::OCCUR_ARBITRARY);
  doc = "Sets data to be written by the participant to preCICE. ";
  doc += "Data is defined by using the <data> tag.";
  tagWriteData.setDocumentation(doc);
  XMLTag tagReadData(*this, TAG_READ, XMLTag::OCCUR_ARBITRARY);
  doc = "Sets data to be read by the participant from preCICE. ";
  doc += "Data is defined by using the <data> tag.";
  tagReadData.setDocumentation(doc);
  auto attrDataName = XMLAttribute<std::string>(ATTR_NAME)
                          .setDocumentation("Name of the data.");
  tagWriteData.addAttribute(attrDataName);
  tagReadData.addAttribute(attrDataName);
  auto attrMesh = XMLAttribute<std::string>(ATTR_MESH)
                      .setDocumentation(
                          "Mesh the data belongs to. If data should be read/written to several "
                          "meshes, this has to be specified separately for each mesh.");
  tagWriteData.addAttribute(attrMesh);
  tagReadData.addAttribute(attrMesh);

  XMLAttribute<int> attrOrder = makeXMLAttribute(ATTR_ORDER, time::Time::DEFAULT_INTERPOLATION_ORDER)
                                    .setDocumentation("Interpolation order used by waveform iteration when reading data.");
  tagReadData.addAttribute(attrOrder);
  tag.addSubtag(tagWriteData);
  tag.addSubtag(tagReadData);

  _mappingConfig = std::make_shared<mapping::MappingConfiguration>(
      tag, _meshConfig);

  _actionConfig = std::make_shared<action::ActionConfiguration>(
      tag, _meshConfig);

  _exportConfig = std::make_shared<io::ExportConfiguration>(tag);

  XMLTag tagWatchPoint(*this, TAG_WATCH_POINT, XMLTag::OCCUR_ARBITRARY);
  doc = "A watch point can be used to follow the transient changes of data ";
  doc += "and mesh vertex coordinates at a given point";
  tagWatchPoint.setDocumentation(doc);
  doc = "Name of the watch point. Is taken in combination with the participant ";
  doc += "name to construct the filename the watch point data is written to.";
  attrName.setDocumentation(doc);
  tagWatchPoint.addAttribute(attrName);
  doc = "Mesh to be watched.";
  attrMesh.setDocumentation(doc);
  tagWatchPoint.addAttribute(attrMesh);
  auto attrCoordinate = XMLAttribute<Eigen::VectorXd>(ATTR_COORDINATE)
                            .setDocumentation(
                                "The coordinates of the watch point. If the watch point is not put exactly "
                                "on the mesh to observe, the closest projection of the point onto the "
                                "mesh is considered instead, and values/coordinates are interpolated "
                                "linearly to that point.");
  tagWatchPoint.addAttribute(attrCoordinate);
  tag.addSubtag(tagWatchPoint);

  auto attrScaleWitConn = XMLAttribute<bool>(ATTR_SCALE_WITH_CONN)
                              .setDocumentation("Whether the vertex data is scaled with the element area before "
                                                "summing up or not. In 2D, vertex data is scaled with the average length of "
                                                "neighboring edges. In 3D, vertex data is scaled with the average surface of "
                                                "neighboring triangles. If false, vertex data is directly summed up.");
  XMLTag tagWatchIntegral(*this, TAG_WATCH_INTEGRAL, XMLTag::OCCUR_ARBITRARY);
  doc = "A watch integral can be used to follow the transient change of integral data ";
  doc += "and surface area for a given coupling mesh.";
  tagWatchIntegral.setDocumentation(doc);
  doc = "Name of the watch integral. Is taken in combination with the participant ";
  doc += "name to construct the filename the watch integral data is written to.";
  attrName.setDocumentation(doc);
  tagWatchIntegral.addAttribute(attrName);
  doc = "Mesh to be watched.";
  attrMesh.setDocumentation(doc);
  tagWatchIntegral.addAttribute(attrMesh);
  tagWatchIntegral.addAttribute(attrScaleWitConn);
  tag.addSubtag(tagWatchIntegral);

  XMLTag tagUseMesh(*this, TAG_USE_MESH, XMLTag::OCCUR_ARBITRARY);
  doc = "Makes a mesh (see tag <mesh> available to a participant.";
  tagUseMesh.setDocumentation(doc);
  attrName.setDocumentation("Name of the mesh.");
  tagUseMesh.addAttribute(attrName);
  //  XMLAttribute<Eigen::VectorXd> attrLocalOffset(ATTR_LOCAL_OFFSET);
  //  doc = "The mesh can have an offset only applied for the local participant. ";
  //  doc += "Vector-valued example: '1.0; 0.0; 0.0'";
  //  attrLocalOffset.setDocumentation(doc);
  //  attrLocalOffset.setDefaultValue(Eigen::VectorXd::Constant(3, 0));
  //  tagUseMesh.addAttribute(attrLocalOffset);

  auto attrFrom = XMLAttribute<std::string>(ATTR_FROM, "")
                      .setDocumentation(
                          "If a created mesh should be used by "
                          "another solver, this attribute has to specify the creating participant's"
                          " name. The creator has to use the attribute \"provide\" to signal he is "
                          "providing the mesh geometry.");
  tagUseMesh.addAttribute(attrFrom);
  auto attrSafetyFactor = makeXMLAttribute(ATTR_SAFETY_FACTOR, 0.5)
                              .setDocumentation(
                                  "If a mesh is received from another partipant (see tag <from>), it needs to be"
                                  "decomposed at the receiving participant. To speed up this process, "
                                  "a geometric filter (see tag <geometric-filter>), i.e. filtering by bounding boxes around the local mesh, can be used. "
                                  "This safety factor defines by which factor this local information is "
                                  "increased. An example: 0.5 means that the bounding box is 150% of its original size.");
  tagUseMesh.addAttribute(attrSafetyFactor);

  auto attrGeoFilter = XMLAttribute<std::string>(ATTR_GEOMETRIC_FILTER)
                           .setDocumentation(
                               "If a mesh is received from another partipant (see tag <from>), it needs to be"
                               "decomposed at the receiving participant. To speed up this process, "
                               "a geometric filter, i.e. filtering by bounding boxes around the local mesh, can be used. "
                               "Two different variants are implemented: a filter \"on-master\" strategy, "
                               "which is beneficial for a huge mesh and a low number of processors, and a filter "
                               "\"on-slaves\" strategy, which performs better for a very high number of "
                               "processors. Both result in the same distribution (if the safety factor is sufficiently large). "
                               "\"on-master\" is not supported if you use two-level initialization. "
                               "For very asymmetric cases, the filter can also be switched off completely (\"no-filter\").")
                           .setOptions({VALUE_FILTER_ON_MASTER, VALUE_FILTER_ON_SLAVES, VALUE_NO_FILTER, VALUE_FILTER_ON_PRIMARY_RANK, VALUE_FILTER_ON_SECONDARY_RANKS})
                           .setDefaultValue(VALUE_FILTER_ON_SECONDARY_RANKS);
  tagUseMesh.addAttribute(attrGeoFilter);

  auto attrDirectAccess = makeXMLAttribute(ATTR_DIRECT_ACCESS, false)
                              .setDocumentation(
                                  "If a mesh is received from another partipant (see tag <from>), it needs to be"
                                  "decomposed at the receiving participant. In case a mapping is defined, the "
                                  "mesh is decomposed according to the local provided mesh associated to the mapping. "
                                  "In case no mapping has been defined (you want to access "
                                  "the mesh and related data direct), there is no obvious way on how to decompose the "
                                  "mesh, since no mesh needs to be provided by the participant. For this purpose, bounding "
                                  "boxes can be defined (see API function \"setMeshAccessRegion\") and used by selecting "
                                  "the option direct-access=\"true\".");

  tagUseMesh.addAttribute(attrDirectAccess);

  auto attrProvide = makeXMLAttribute(ATTR_PROVIDE, false)
                         .setDocumentation(
                             "If this attribute is set to \"on\", the "
                             "participant has to create the mesh geometry before initializing preCICE.");
  tagUseMesh.addAttribute(attrProvide);
  tag.addSubtag(tagUseMesh);

  std::list<XMLTag>  intraCommTags;
  XMLTag::Occurrence intraCommOcc = XMLTag::OCCUR_NOT_OR_ONCE;
  for (std::string tag_name : {TAG_MASTER, TAG_INTRA_COMM}) {
    {
      XMLTag tagIntraComm(*this, "sockets", intraCommOcc, tag_name);
      doc = "A solver in parallel needs a communication between its ranks. ";
      doc += "By default, the participant's MPI_COM_WORLD is reused.";
      doc += "Use this tag to use TCP/IP sockets instead.";
      tagIntraComm.setDocumentation(doc);

      auto attrPort = makeXMLAttribute("port", 0)
                          .setDocumentation(
                              "Port number (16-bit unsigned integer) to be used for socket "
                              "communication. The default is \"0\", what means that OS will "
                              "dynamically search for a free port (if at least one exists) and "
                              "bind it automatically.");
      tagIntraComm.addAttribute(attrPort);

      auto attrNetwork = makeXMLAttribute(ATTR_NETWORK, utils::networking::loopbackInterfaceName())
                             .setDocumentation(
                                 "Interface name to be used for socket communication. "
                                 "Default is the canonical name of the loopback interface of your platform. "
                                 "Might be different on supercomputing systems, e.g. \"ib0\" "
                                 "for the InfiniBand on SuperMUC. ");
      tagIntraComm.addAttribute(attrNetwork);

      auto attrExchangeDirectory = makeXMLAttribute(ATTR_EXCHANGE_DIRECTORY, "")
                                       .setDocumentation(
                                           "Directory where connection information is exchanged. By default, the "
                                           "directory of startup is chosen.");
      tagIntraComm.addAttribute(attrExchangeDirectory);

      intraCommTags.push_back(tagIntraComm);
    }
    {
      XMLTag tagIntraComm(*this, "mpi", intraCommOcc, tag_name);
      doc = "A solver in parallel needs a communication between its ranks. ";
      doc += "By default, the participant's MPI_COM_WORLD is reused.";
      doc += "Use this tag to use MPI with separated communication spaces instead instead.";
      tagIntraComm.setDocumentation(doc);

      auto attrExchangeDirectory = makeXMLAttribute(ATTR_EXCHANGE_DIRECTORY, "")
                                       .setDocumentation(
                                           "Directory where connection information is exchanged. By default, the "
                                           "directory of startup is chosen.");
      tagIntraComm.addAttribute(attrExchangeDirectory);

      intraCommTags.push_back(tagIntraComm);
    }
    {
      XMLTag tagIntraComm(*this, "mpi-single", intraCommOcc, tag_name);
      doc = "A solver in parallel needs a communication between its ranks. ";
      doc += "By default (which is this option), the participant's MPI_COM_WORLD is reused.";
      doc += "This tag is only used to ensure backwards compatibility.";
      tagIntraComm.setDocumentation(doc);

      intraCommTags.push_back(tagIntraComm);
    }
    for (XMLTag &tagIntraComm : intraCommTags) {
      tag.addSubtag(tagIntraComm);
    }
  }
  parent.addSubtag(tag);
}

void ParticipantConfiguration::setDimensions(
    int dimensions)
{
  PRECICE_TRACE(dimensions);
  PRECICE_ASSERT((dimensions == 2) || (dimensions == 3), dimensions);
  _dimensions = dimensions;
}

void ParticipantConfiguration::setExperimental(
    bool experimental)
{
  _experimental = experimental;
}

void ParticipantConfiguration::xmlTagCallback(
    const xml::ConfigurationContext &context,
    xml::XMLTag &                    tag)
{
  PRECICE_TRACE(tag.getName());
  if (tag.getName() == TAG) {
    const std::string &  name = tag.getStringAttributeValue(ATTR_NAME);
    impl::PtrParticipant p(new impl::Participant(name, _meshConfig));
    _participants.push_back(p);
  } else if (tag.getName() == TAG_USE_MESH) {
    PRECICE_ASSERT(_dimensions != 0); // setDimensions() has been called
    std::string     name = tag.getStringAttributeValue(ATTR_NAME);
    Eigen::VectorXd offset(_dimensions);
    /// @todo offset currently not supported
    //offset = tag.getEigenVectorXdAttributeValue(ATTR_LOCAL_OFFSET, _dimensions);
    std::string                                   from              = tag.getStringAttributeValue(ATTR_FROM);
    double                                        safetyFactor      = tag.getDoubleAttributeValue(ATTR_SAFETY_FACTOR);
    partition::ReceivedPartition::GeometricFilter geoFilter         = getGeoFilter(tag.getStringAttributeValue(ATTR_GEOMETRIC_FILTER));
    const bool                                    allowDirectAccess = tag.getBooleanAttributeValue(ATTR_DIRECT_ACCESS);

    if (allowDirectAccess) {
      if (!_experimental) {
        PRECICE_ERROR("You tried to configure the received mesh \"{}\" to use the option access-direct=\"true\", which is currently still experimental. Please set experimental=\"true\", if you want to use this feature.", name);
      }
      PRECICE_WARN("You configured the received mesh \"{}\" to use the option access-direct=\"true\", which is currently still experimental. Use with care.", name);
    }

    PRECICE_CHECK(safetyFactor >= 0,
                  "Participant \"{}\" uses mesh \"{}\" with safety-factor=\"{}\". "
                  "Please use a positive or zero safety-factor instead.",
                  context.name, name, safetyFactor);

    bool provide = tag.getBooleanAttributeValue(ATTR_PROVIDE);
    if (_participants.back()->getName() == from) {
      PRECICE_CHECK(provide,
                    "Participant \"{}\" cannot use mesh \"{}\" from itself. "
                    "Use the \"from\"-field to specify which participant has to communicate the mesh to \"{}\".",
                    context.name, name, context.name);
    }
    mesh::PtrMesh mesh = _meshConfig->getMesh(name);
    PRECICE_CHECK(mesh,
                  "Participant \"{}\" uses mesh \"{}\" which is not defined. "
                  "Please check the use-mesh node with name=\"{}\" or define the mesh.",
                  _participants.back()->getName(), name, name);
    if ((geoFilter != partition::ReceivedPartition::GeometricFilter::ON_SECONDARY_RANKS || safetyFactor != 0.5) && from == "") {
      PRECICE_ERROR(
          "Participant \"{}\" uses mesh \"{}\", which is not received (no \"from\"), but has a geometric-filter and/or a safety factor defined. "
          "Please extend the use-mesh tag as follows: <use-mesh name=\"{}\" from=\"(other participant)\" />",
          _participants.back()->getName(), name, name);
    }

    PRECICE_CHECK(!(allowDirectAccess && from.empty()),
                  "Participant \"{}\" uses mesh \"{}\", which is not received (no \"from\"), but has a direct access defined. "
                  "This combination of options is not allowed. "
                  "Please extend the use-mesh tag as follows: <use-mesh name=\"{}\" from=\"(other participant)\" />"
                  " or remove the direct access option.",
                  _participants.back()->getName(), name, name);

    _participants.back()->useMesh(mesh, offset, false, from, safetyFactor, provide, geoFilter, allowDirectAccess);
  } else if (tag.getName() == TAG_WRITE) {
    const std::string &dataName = tag.getStringAttributeValue(ATTR_NAME);
    std::string        meshName = tag.getStringAttributeValue(ATTR_MESH);
    mesh::PtrMesh      mesh     = _meshConfig->getMesh(meshName);
    PRECICE_CHECK(mesh,
                  "Participant \"{}\" has to use mesh \"{}\" in order to write data to it. Please add a use-mesh node with name=\"{}\".",
                  _participants.back()->getName(), meshName, meshName);
    mesh::PtrData data = getData(mesh, dataName);
    _participants.back()->addWriteData(data, mesh);
  } else if (tag.getName() == TAG_READ) {
    const std::string &dataName = tag.getStringAttributeValue(ATTR_NAME);
    std::string        meshName = tag.getStringAttributeValue(ATTR_MESH);
    mesh::PtrMesh      mesh     = _meshConfig->getMesh(meshName);
    PRECICE_CHECK(mesh,
                  "Participant \"{}\" has to use mesh \"{}\" in order to read data from it. Please add a use-mesh node with name=\"{}\".",
                  _participants.back()->getName(), meshName, meshName);
    mesh::PtrData data          = getData(mesh, dataName);
    int           waveformOrder = tag.getIntAttributeValue(ATTR_ORDER);
    if (waveformOrder != time::Time::DEFAULT_INTERPOLATION_ORDER) {
      if (!_experimental) {
        PRECICE_ERROR("You tried to configure the read data with name \"{}\" to use the waveform-order=\"{}\", which is currently still experimental. Please set experimental=\"true\", if you want to use this feature.", dataName, waveformOrder);
      }
      if (waveformOrder < time::Time::MIN_INTERPOLATION_ORDER || waveformOrder > time::Time::MAX_INTERPOLATION_ORDER) {
        PRECICE_ERROR("You tried to configure the read data with name \"{}\" to use the waveform-order=\"{}\", but the order must be between \"{}\" and \"{}\". Please use an order in the allowed range.", dataName, waveformOrder, time::Time::MIN_INTERPOLATION_ORDER, time::Time::MAX_INTERPOLATION_ORDER);
      }
      PRECICE_WARN("You configured the read data with name \"{}\" to use the waveform-order=\"{}\", which is currently still experimental. Use with care.", dataName, waveformOrder);
    }
    _participants.back()->addReadData(data, mesh, waveformOrder);
  } else if (tag.getName() == TAG_WATCH_POINT) {
    PRECICE_ASSERT(_dimensions != 0); // setDimensions() has been called
    WatchPointConfig config;
    config.name        = tag.getStringAttributeValue(ATTR_NAME);
    config.nameMesh    = tag.getStringAttributeValue(ATTR_MESH);
    config.coordinates = tag.getEigenVectorXdAttributeValue(ATTR_COORDINATE, _dimensions);
    _watchPointConfigs.push_back(config);
  } else if (tag.getName() == TAG_WATCH_INTEGRAL) {
    PRECICE_ASSERT(_dimensions != 0);
    WatchIntegralConfig config;
    config.name        = tag.getStringAttributeValue(ATTR_NAME);
    config.nameMesh    = tag.getStringAttributeValue(ATTR_MESH);
    config.isScalingOn = tag.getBooleanAttributeValue(ATTR_SCALE_WITH_CONN);
    _watchIntegralConfigs.push_back(config);
  } else if (tag.getNamespace() == TAG_MASTER || tag.getNamespace() == TAG_INTRA_COMM) {
    if (tag.getNamespace() == TAG_MASTER) {
      PRECICE_WARN("Tag \"{}\" is deprecated and will be removed in v3.0.0. Please use \"{}\".", TAG_MASTER, TAG_INTRA_COMM);
    }
    com::CommunicationConfiguration comConfig;
    com::PtrCommunication           com  = comConfig.createCommunication(tag);
    utils::IntraComm::getCommunication() = com;
    _isIntraCommDefined                  = true;
    _participants.back()->setUsePrimaryRank(true);
  }
}

void ParticipantConfiguration::xmlEndTagCallback(
    const xml::ConfigurationContext &context,
    xml::XMLTag &                    tag)
{
  if (tag.getName() == TAG) {
    finishParticipantConfiguration(context, _participants.back());
  }
}

const std::vector<impl::PtrParticipant> &
ParticipantConfiguration::getParticipants() const
{
  return _participants;
}

partition::ReceivedPartition::GeometricFilter ParticipantConfiguration::getGeoFilter(const std::string &geoFilter) const
{
  if (geoFilter == VALUE_FILTER_ON_MASTER || geoFilter == VALUE_FILTER_ON_PRIMARY_RANK) {
    if (geoFilter == VALUE_FILTER_ON_MASTER) {
      PRECICE_WARN("Value \"{}\" is deprecated and will be removed in v3.0.0. Please use \"{}\"", VALUE_FILTER_ON_MASTER, VALUE_FILTER_ON_PRIMARY_RANK);
    }
    return partition::ReceivedPartition::GeometricFilter::ON_PRIMARY_RANK;
  } else if (geoFilter == VALUE_FILTER_ON_SLAVES || geoFilter == VALUE_FILTER_ON_SECONDARY_RANKS) {
    if (geoFilter == VALUE_FILTER_ON_SLAVES) {
      PRECICE_WARN("Value \"{}\" is deprecated and will be removed in v3.0.0. Please use \"{}\".", VALUE_FILTER_ON_SLAVES, VALUE_FILTER_ON_SECONDARY_RANKS);
    }
    return partition::ReceivedPartition::GeometricFilter::ON_SECONDARY_RANKS;
  } else {
    PRECICE_ASSERT(geoFilter == VALUE_NO_FILTER);
    return partition::ReceivedPartition::GeometricFilter::NO_FILTER;
  }
}

const mesh::PtrData &ParticipantConfiguration::getData(
    const mesh::PtrMesh &mesh,
    const std::string &  nameData) const
{
  PRECICE_CHECK(mesh->hasDataName(nameData),
                "Participant \"{}\" asks for data \"{}\" from mesh \"{}\", but this mesh does not use such data. "
                "Please add a use-data tag with name=\"{}\" to this mesh.",
                _participants.back()->getName(), nameData, mesh->getName(), nameData);
  return mesh->data(nameData);
}

void ParticipantConfiguration::finishParticipantConfiguration(
    const xml::ConfigurationContext &context,
    const impl::PtrParticipant &     participant)
{
  PRECICE_TRACE(participant->getName());

  // Set input/output meshes for data mappings and mesh requirements
  using ConfMapping = mapping::MappingConfiguration::ConfiguredMapping;
  for (const ConfMapping &confMapping : _mappingConfig->mappings()) {

    checkIllDefinedMappings(confMapping, participant);

    int fromMeshID = confMapping.fromMesh->getID();
    int toMeshID   = confMapping.toMesh->getID();

    PRECICE_CHECK(participant->isMeshUsed(fromMeshID),
                  "Participant \"{}\" has mapping from mesh \"{}\", without using this mesh. "
                  "Please add a use-mesh tag with name=\"{}\"",
                  participant->getName(), confMapping.fromMesh->getName(), confMapping.fromMesh->getName());
    PRECICE_CHECK(participant->isMeshUsed(toMeshID),
                  "Participant \"{}\" has mapping to mesh \"{}\", without using this mesh. "
                  "Please add a use-mesh tag with name=\"{}\"",
                  participant->getName(), confMapping.toMesh->getName(), confMapping.toMesh->getName());
    PRECICE_CHECK((participant->isMeshProvided(fromMeshID) || participant->isMeshProvided(toMeshID)),
                  "Participant \"{}\" has mapping from mesh \"{}\",  to mesh \"{}\", but neither are provided. "
                  "Please mark the mesh provided by this participant by configuring its use-mesh tag with provided=\"true\".",
                  participant->getName(), confMapping.fromMesh->getName(), confMapping.toMesh->getName());

    if (context.size > 1) {
      if ((confMapping.direction == mapping::MappingConfiguration::WRITE &&
           confMapping.mapping->getConstraint() == mapping::Mapping::CONSISTENT) ||
          (confMapping.direction == mapping::MappingConfiguration::READ &&
           confMapping.mapping->getConstraint() == mapping::Mapping::CONSERVATIVE)) {
        PRECICE_ERROR("For a parallel participant, only the mapping combinations read-consistent and write-conservative are allowed");
      } else if (confMapping.mapping->getConstraint() == mapping::Mapping::SCALEDCONSISTENT) {
        PRECICE_ERROR("Scaled consistent mapping is not yet supported for a parallel participant. "
                      "You could run in serial or use a plain (read-)consistent mapping instead.");
      }
    }

    impl::MeshContext &fromMeshContext = participant->meshContext(fromMeshID);
    impl::MeshContext &toMeshContext   = participant->meshContext(toMeshID);

    if (confMapping.direction == mapping::MappingConfiguration::READ) {
      PRECICE_CHECK(toMeshContext.provideMesh,
                    "A read mapping of participant \"{}\" needs to map TO a provided mesh. Mesh \"{}\" is not provided. "
                    "Please add a provide=\"yes\" attribute to the participant's use-mesh tag.",
                    participant->getName(), confMapping.toMesh->getName());
      PRECICE_CHECK(not fromMeshContext.receiveMeshFrom.empty(),
                    "A read mapping of participant \"{}\" needs to map FROM a received mesh. Mesh \"{}\" is not received. "
                    "Please add a from=\"(participant)\" attribute to the participant's use-mesh tag.",
                    participant->getName(), confMapping.fromMesh->getName());
    } else {
      PRECICE_CHECK(fromMeshContext.provideMesh,
                    "A write mapping of participant \"{}\" needs to map FROM a provided mesh. Mesh \"{}\" is not provided. "
                    "Please add a provide=\"yes\" attribute to the participant's use-mesh tag.",
                    participant->getName(), confMapping.fromMesh->getName());
      PRECICE_CHECK(not toMeshContext.receiveMeshFrom.empty(),
                    "A write mapping of participant \"{}\" needs to map TO a received mesh. Mesh \"{}\" is not received. "
                    "Please add a from=\"(participant)\" attribute to the participant's use-mesh tag.",
                    participant->getName(), confMapping.toMesh->getName());
    }

    if (confMapping.isRBF) {
      fromMeshContext.geoFilter = partition::ReceivedPartition::GeometricFilter::NO_FILTER;
      toMeshContext.geoFilter   = partition::ReceivedPartition::GeometricFilter::NO_FILTER;
    }

    precice::impl::MappingContext *mappingContext = new precice::impl::MappingContext();
    mappingContext->fromMeshID                    = fromMeshID;
    mappingContext->toMeshID                      = toMeshID;
    mappingContext->timing                        = confMapping.timing;

    mapping::PtrMapping &map = mappingContext->mapping;
    PRECICE_ASSERT(map.get() == nullptr);
    map = confMapping.mapping;

    const mesh::PtrMesh &input  = fromMeshContext.mesh;
    const mesh::PtrMesh &output = toMeshContext.mesh;
    PRECICE_DEBUG("Configure mapping for input={}, output={}", input->getName(), output->getName());
    map->setMeshes(input, output);

    if (confMapping.direction == mapping::MappingConfiguration::WRITE) {
      participant->addWriteMappingContext(mappingContext);
    } else {
      PRECICE_ASSERT(confMapping.direction == mapping::MappingConfiguration::READ);
      participant->addReadMappingContext(mappingContext);
    }

    fromMeshContext.meshRequirement = std::max(
        fromMeshContext.meshRequirement, map->getInputRequirement());
    toMeshContext.meshRequirement = std::max(
        toMeshContext.meshRequirement, map->getOutputRequirement());

    fromMeshContext.fromMappingContexts.push_back(*mappingContext);
    toMeshContext.toMappingContexts.push_back(*mappingContext);
  }
  _mappingConfig->resetMappings();

  // Iterate over all write mappings
  for (impl::MappingContext &mappingContext : participant->writeMappingContexts()) {
    // Check, whether we can find a corresponding write data context
    bool dataFound = false;
    for (auto &dataContext : participant->writeDataContexts()) {
      // First we look for the "from" mesh ID
      const int fromMeshID = dataContext.getMeshID();
      if (mappingContext.fromMeshID == fromMeshID) {
        // Second we look for the "to" mesh ID
        impl::MeshContext &meshContext = participant->meshContext(mappingContext.toMeshID);
        // If this is true, we actually found a proper configuration
        // If it is false, we look for another "from" mesh ID, because we might have multiple read and write mappings
        if (meshContext.mesh->hasDataName(dataContext.getDataName())) {
          // Check, if the fromMesh is a provided mesh
          PRECICE_CHECK(participant->isMeshProvided(fromMeshID),
                        "Participant \"{}\" has to use and provide mesh \"{}\" to be able to write data to it. "
                        "Please add a use-mesh node with name=\"{}\" and provide=\"true\".",
                        participant->getName(), dataContext.getMeshName(), dataContext.getMeshName());
          dataContext.appendMappingConfiguration(mappingContext, meshContext);
          // Enable gradient data if required
          if (mappingContext.mapping->requiresGradientData() == true) {
            mappingContext.requireGradientData(dataContext.getDataName());
          }
          dataFound = true;
        }
      }
    }
    PRECICE_CHECK(dataFound,
                  "Participant \"{}\" defines a write mapping from mesh \"{}\" to mesh \"{}\", "
                  "but there is either no corresponding write-data tag or the meshes used "
                  "by this participant lack the necessary use-data tags.",
                  participant->getName(), mappingContext.mapping->getInputMesh()->getName(), mappingContext.mapping->getOutputMesh()->getName());
  }

  // Iterate over all read mappings
  for (impl::MappingContext &mappingContext : participant->readMappingContexts()) {
    // Check, weather we can find a corresponding read data context
    bool dataFound = false;
    for (auto &dataContext : participant->readDataContexts()) {
      // First we look for the "to" mesh ID
      const int toMeshID = dataContext.getMeshID();
      if (mappingContext.toMeshID == toMeshID) {
        // Second we look for the "from" mesh ID
        impl::MeshContext &meshContext = participant->meshContext(mappingContext.fromMeshID);
        // If this is true, we actually found a proper configuration
        // If it is false, we look for another "from" mesh ID, because we might have multiple read and write mappings
        if (meshContext.mesh->hasDataName(dataContext.getDataName())) {
          // Check, if the toMesh is a provided mesh
          PRECICE_CHECK(participant->isMeshProvided(toMeshID),
                        "Participant \"{}\" has to use and provide mesh \"{}\" in order to read data from it. "
                        "Please add a use-mesh node with name=\"{}\" and provide=\"true\".",
                        participant->getName(), dataContext.getMeshName(), dataContext.getMeshName());
          dataContext.appendMappingConfiguration(mappingContext, meshContext);
          // Enable gradient data if required
          if (mappingContext.mapping->requiresGradientData() == true) {
            mappingContext.requireGradientData(dataContext.getDataName());
          }
          dataFound = true;
        }
      }
    }
    PRECICE_CHECK(dataFound,
                  "Participant \"{}\" defines a read mapping from mesh \"{}\" to mesh \"{}\", "
                  "but there is either no corresponding read-data tag or the meshes used "
                  "by this participant lack the necessary use-data tags.",
                  participant->getName(), mappingContext.mapping->getInputMesh()->getName(), mappingContext.mapping->getOutputMesh()->getName());
  }

  // Add actions
  for (const action::PtrAction &action : _actionConfig->actions()) {
    bool used = _participants.back()->isMeshUsed(action->getMesh()->getID());
    PRECICE_CHECK(used,
                  "Data action of participant \"{}\" uses mesh \"{}\", which is not used by the participant. "
                  "Please add a use-mesh node with name=\"{}\".",
                  _participants.back()->getName(), action->getMesh()->getName(), action->getMesh()->getName());
  }
  for (action::PtrAction &action : _actionConfig->extractActions()) {
    _participants.back()->addAction(std::move(action));
  }

  // Add export contexts
  for (io::ExportContext &exportContext : _exportConfig->exportContexts()) {
    io::PtrExport exporter;
    if (exportContext.type == VALUE_VTK) {
      // This is handled with respect to the current configuration context.
      // Hence, this is potentially wrong for every participant other than context.name.
      if (context.size > 1) {
        // Only display the warning message if this participant configuration is the current one.
        if (context.name == participant->getName()) {
          PRECICE_WARN("You are using the VTK exporter in the parallel participant {}. "
                       "Note that this will export as PVTU instead. For consistency, prefer \"<export:vtu ... />\" instead.",
                       participant->getName());
        }
        exporter = io::PtrExport(new io::ExportVTU());
      } else {
        exporter = io::PtrExport(new io::ExportVTK());
      }
    } else if (exportContext.type == VALUE_VTU) {
      exporter = io::PtrExport(new io::ExportVTU());
    } else if (exportContext.type == VALUE_VTP) {
      exporter = io::PtrExport(new io::ExportVTP());
    } else if (exportContext.type == VALUE_CSV) {
      exporter = io::PtrExport(new io::ExportCSV());
    } else {
      PRECICE_ERROR("Participant {} defines an <export/> tag of unknown type \"{}\".",
                    _participants.back()->getName(), exportContext.type);
    }
    exportContext.exporter = exporter;

    _participants.back()->addExportContext(exportContext);
  }
  _exportConfig->resetExports();

  // Create watch points
  for (const WatchPointConfig &config : _watchPointConfigs) {
    PRECICE_CHECK(participant->isMeshUsed(config.nameMesh),
                  "Participant \"{}\" defines watchpoint \"{}\" for mesh \"{}\" which is not used by the participant. "
                  "Please add a use-mesh node with name=\"{}\".",
                  participant->getName(), config.name, config.nameMesh, config.nameMesh);
    const auto &meshContext = participant->usedMeshContext(config.nameMesh);
    PRECICE_CHECK(meshContext.provideMesh,
                  "Participant \"{}\" defines watchpoint \"{}\" for the received mesh \"{}\", which is not allowed. "
                  "Please move the watchpoint definition to the participant providing mesh \"{}\".",
                  participant->getName(), config.name, config.nameMesh, config.nameMesh);

    std::string         filename = "precice-" + participant->getName() + "-watchpoint-" + config.name + ".log";
    impl::PtrWatchPoint watchPoint(new impl::WatchPoint(config.coordinates, meshContext.mesh, filename));
    participant->addWatchPoint(watchPoint);
  }
  _watchPointConfigs.clear();

  // Create watch integrals
  for (const WatchIntegralConfig &config : _watchIntegralConfigs) {
    PRECICE_CHECK(participant->isMeshUsed(config.nameMesh),
                  "Participant \"{}\" defines watch integral \"{}\" for mesh \"{}\" which is not used by the participant. "
                  "Please add a use-mesh node with name=\"{}\".",
                  participant->getName(), config.name, config.nameMesh, config.nameMesh);
    const auto &meshContext = participant->usedMeshContext(config.nameMesh);
    PRECICE_CHECK(meshContext.provideMesh,
                  "Participant \"{}\" defines watch integral \"{}\" for the received mesh \"{}\", which is not allowed. "
                  "Please move the watchpoint definition to the participant providing mesh \"{}\".",
                  participant->getName(), config.name, config.nameMesh, config.nameMesh);

    std::string            filename = "precice-" + participant->getName() + "-watchintegral-" + config.name + ".log";
    impl::PtrWatchIntegral watchIntegral(new impl::WatchIntegral(meshContext.mesh, filename, config.isScalingOn));
    participant->addWatchIntegral(watchIntegral);
  }
  _watchIntegralConfigs.clear();

  // create default primary communication if needed
  if (context.size > 1 && not _isIntraCommDefined && participant->getName() == context.name) {
#ifdef PRECICE_NO_MPI
    PRECICE_ERROR("Implicit intra-participant communications for parallel participants are only available if preCICE was built with MPI. "
                  "Either explicitly define an intra-participant communication for each parallel participant or rebuild preCICE with \"PRECICE_MPICommunication=ON\".");
#else
    com::PtrCommunication com            = std::make_shared<com::MPIDirectCommunication>();
    utils::IntraComm::getCommunication() = com;
    participant->setUsePrimaryRank(true);
#endif
  }
  _isIntraCommDefined = false; // to not mess up with previous participant
}

void ParticipantConfiguration::checkIllDefinedMappings(
    const mapping::MappingConfiguration::ConfiguredMapping &mapping,
    const impl::PtrParticipant &                            participant)
{
  PRECICE_TRACE();
  using ConfMapping = mapping::MappingConfiguration::ConfiguredMapping;

  for (const ConfMapping &configuredMapping : _mappingConfig->mappings()) {
    bool sameToMesh   = mapping.toMesh->getName() == configuredMapping.toMesh->getName();
    bool sameFromMesh = mapping.fromMesh->getName() == configuredMapping.fromMesh->getName();
    if (sameToMesh && sameFromMesh) {
      // It's really the same mapping, not a duplicated one. Those are already checked for in MappingConfiguration.
      return;
    }

    if (sameToMesh) {
      for (const mesh::PtrData &data : mapping.fromMesh->data()) {
        for (const mesh::PtrData &configuredData : configuredMapping.fromMesh->data()) {
          bool sameFromData = data->getName() == configuredData->getName();

          if (not sameFromData) {
            continue;
          }

          bool sameDirection = false;

          if (mapping.direction == mapping::MappingConfiguration::WRITE) {
            for (const auto &dataContext : participant->writeDataContexts()) {
              sameDirection |= data->getName() == dataContext.getDataName();
            }
          }
          if (mapping.direction == mapping::MappingConfiguration::READ) {
            for (const auto &dataContext : participant->readDataContexts()) {
              sameDirection |= data->getName() == dataContext.getDataName();
            }
          }
          PRECICE_CHECK(!sameDirection,
                        "There cannot be two mappings to mesh \"{}\" if the meshes from which is mapped contain "
                        "duplicated data fields that are also actually mapped on this participant. "
                        "Here, both from meshes contain data \"{}\". "
                        "The mapping is not well defined. "
                        "Which data \"{}\" should be mapped to mesh \"{}\"?",
                        mapping.toMesh->getName(), data->getName(), data->getName(), mapping.toMesh->getName());
        }
      }
    }
  }
}

} // namespace precice::config

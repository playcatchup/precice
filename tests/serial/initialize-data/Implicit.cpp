#ifndef PRECICE_NO_MPI

#include "testing/Testing.hpp"

#include <precice/SolverInterface.hpp>
#include <vector>

using namespace precice;

BOOST_AUTO_TEST_SUITE(Integration)
BOOST_AUTO_TEST_SUITE(Serial)
BOOST_AUTO_TEST_SUITE(InitializeData)
/**
 * @brief Test simple coupled simulation with iterations, data initialization and without acceleration
 *
 */
BOOST_AUTO_TEST_CASE(Implicit)
{
  PRECICE_TEST("SolverOne"_on(1_rank), "SolverTwo"_on(1_rank));

  using namespace precice::constants;

  SolverInterface couplingInterface(context.name, context.config(), 0, 1);

  int         dimensions = couplingInterface.getDimensions();
  std::string meshName;
  std::string writeDataName;
  std::string readDataName;
  double      writeValue, expectedReadValue;

  if (context.isNamed("SolverOne")) {
    meshName          = "MeshOne";
    writeDataName     = "Forces";
    readDataName      = "Velocities";
    writeValue        = 1;
    expectedReadValue = 2;
  } else {
    BOOST_TEST(context.isNamed("SolverTwo"));
    meshName          = "MeshTwo";
    writeDataName     = "Velocities";
    readDataName      = "Forces";
    writeValue        = 2;
    expectedReadValue = 1;
  }
  int                 meshID      = couplingInterface.getMeshID(meshName);
  int                 writeDataID = couplingInterface.getDataID(writeDataName, meshID);
  int                 readDataID  = couplingInterface.getDataID(readDataName, meshID);
  std::vector<double> vertex(dimensions, 0);
  int                 vertexID = couplingInterface.setMeshVertex(meshID, vertex.data());

  double dt = 0;
  dt        = couplingInterface.initialize();
  std::vector<double> writeData(dimensions, writeValue);
  std::vector<double> readData(dimensions, -1);
  const std::string & cowid = actionWriteInitialData();

  if (couplingInterface.isActionRequired(cowid)) {
    BOOST_TEST(context.isNamed("SolverTwo"));
    couplingInterface.writeVectorData(writeDataID, vertexID, writeData.data());
    couplingInterface.markActionFulfilled(cowid);
  }

  couplingInterface.initializeData();

  while (couplingInterface.isCouplingOngoing()) {
    if (couplingInterface.isActionRequired(actionWriteIterationCheckpoint())) {
      couplingInterface.markActionFulfilled(actionWriteIterationCheckpoint());
    }
    couplingInterface.readVectorData(readDataID, vertexID, readData.data());
    BOOST_TEST(expectedReadValue == readData.at(0));
    BOOST_TEST(expectedReadValue == readData.at(1));
    couplingInterface.writeVectorData(writeDataID, vertexID, writeData.data());
    dt = couplingInterface.advance(dt);
    if (couplingInterface.isActionRequired(actionReadIterationCheckpoint())) {
      couplingInterface.markActionFulfilled(actionReadIterationCheckpoint());
    }
  }
  couplingInterface.finalize();
}

BOOST_AUTO_TEST_SUITE_END() // Integration
BOOST_AUTO_TEST_SUITE_END() // Serial
BOOST_AUTO_TEST_SUITE_END() // InitializeData

#endif // PRECICE_NO_MPI

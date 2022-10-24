#include "precice/SolverInterfaceC.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
  double      dt                 = 0.0;
  int         solverProcessIndex = 0;
  int         solverProcessSize  = 1;
  int         dimensions         = -1;
  double *    vertices;
  double *    readData;
  double *    writeData;
  int         meshID = -1;
  int         dataID = -1;
  int *       vertexIDs;
  int         numberOfVertices = 3;
  int         writeDataID      = -1;
  int         readDataID       = -1;
  const char *meshName;
  const char *writeDataName;
  const char *readDataName;

  if (argc != 3) {
    printf("The solverdummy was called with an incorrect number of arguments. Usage: ./solverdummy configFile solverName\n\n");
    printf("Parameter description\n");
    printf("  configurationFile: Path and filename of preCICE configuration\n");
    printf("  solverName:        SolverDummy participant name in preCICE configuration\n");
    return 1;
  }

  const char *configFileName  = argv[1];
  const char *participantName = argv[2];

  printf("DUMMY: Running solver dummy with preCICE config file \"%s\" and participant name \"%s\".\n",
         configFileName, participantName);

  const char *writeItCheckp = precicec_actionWriteIterationCheckpoint();
  const char *readItCheckp  = precicec_actionReadIterationCheckpoint();

  precicec_createSolverInterface(participantName, configFileName, solverProcessIndex, solverProcessSize);

  if (strcmp(participantName, "SolverOne") == 0) {
    writeDataName = "dataOne";
    readDataName  = "dataTwo";
    meshName      = "MeshOne";
  }
  if (strcmp(participantName, "SolverTwo") == 0) {
    writeDataName = "dataTwo";
    readDataName  = "dataOne";
    meshName      = "MeshTwo";
  }

  meshID      = precicec_getMeshID(meshName);
  writeDataID = precicec_getDataID(writeDataName, meshID);
  readDataID  = precicec_getDataID(readDataName, meshID);

  dimensions = precicec_getDimensions();
  vertices   = malloc(numberOfVertices * dimensions * sizeof(double));
  readData   = malloc(numberOfVertices * dimensions * sizeof(double));
  writeData  = malloc(numberOfVertices * dimensions * sizeof(double));
  vertexIDs  = malloc(numberOfVertices * sizeof(int));

  for (int i = 0; i < numberOfVertices; i++) {
    for (int j = 0; j < dimensions; j++) {
      vertices[j + dimensions * i]  = i;
      readData[j + dimensions * i]  = i;
      writeData[j + dimensions * i] = i;
    }
  }

  precicec_setMeshVertices(meshID, numberOfVertices, vertices, vertexIDs);

  free(vertices);

  dt = precicec_initialize();

  while (precicec_isCouplingOngoing()) {

    if (precicec_isActionRequired(writeItCheckp)) {
      printf("DUMMY: Writing iteration checkpoint \n");
      precicec_markActionFulfilled(writeItCheckp);
    }

    if (precicec_isReadDataAvailable) {
      precicec_readBlockVectorData(readDataID, numberOfVertices, vertexIDs, readData);
    }

    for (int i = 0; i < numberOfVertices * dimensions; i++) {
      writeData[i] = readData[i] + 1;
    }

    if (precicec_isWriteDataRequired(dt)) {
      precicec_writeBlockVectorData(writeDataID, numberOfVertices, vertexIDs, writeData);
    }

    dt = precicec_advance(dt);

    if (precicec_isActionRequired(readItCheckp)) {
      printf("DUMMY: Reading iteration checkpoint \n");
      precicec_markActionFulfilled(readItCheckp);
    } else {
      printf("DUMMY: Advancing in time \n");
    }
  }

  precicec_finalize();
  free(writeData);
  free(readData);
  free(vertexIDs);
  printf("DUMMY: Closing C solver dummy... \n");

  return 0;
}

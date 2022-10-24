#ifndef PRECICE_NO_PYTHON

#include "PythonAction.hpp"
#include <Eigen/Core>
#include <Python.h>
#include <boost/filesystem/operations.hpp>
#include <cstdlib>
#include <memory>
#include <ostream>
#include <pthread.h>
#include <string>
#include <utility>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#include "logging/LogMacros.hpp"
#include "mesh/Data.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/Vertex.hpp"
#include "utils/String.hpp"
#include "utils/assertion.hpp"

namespace precice::action {

namespace {
std::string python_error_as_string()
{
  PyObject *ptype, *pvalue, *ptraceback;
  PyErr_Fetch(&ptype, &pvalue, &ptraceback);
  if (ptype == nullptr) {
    return "<no error available>";
  } else {
    // pvalue and ptraceback may be NULL
    // We don't need the type or the traceback, so we dereference them straight away
    Py_DECREF(ptype);
    Py_XDECREF(ptraceback); // may be NULL

    if (pvalue == nullptr) {
      return "<no error message available>";
    }
    wchar_t *wmessage = PyUnicode_AsWideCharString(pvalue, nullptr);
    Py_DECREF(pvalue);

    if (wmessage) {
      auto message = utils::truncate_wstring_to_string(wmessage);
      PyMem_Free(wmessage);
      return message;
    } else {
      return "<fetching error message failed>";
    }
  }
}

/// Fetches the function inspect.getfullargspec().
PyObject *getfullargspec()
{
  PyObject *const inspect_module_name     = PyUnicode_DecodeFSDefault("inspect");
  PyObject *const inspect_module          = PyImport_Import(inspect_module_name);
  PyObject *const getfullargspec_function = PyObject_GetAttrString(inspect_module, "getfullargspec");
  Py_DECREF(inspect_module_name);
  Py_DECREF(inspect_module);
  return getfullargspec_function;
}

/// Returns the argument names of a callable
std::vector<std::string> python_func_args(PyObject *const func)
{
  PyObject *const getfullargspec_function = getfullargspec();

  // Call the inspect.getfullargspec function.
  PyObject *const argspec_call_args = PyTuple_New(1);
  PyTuple_SetItem(argspec_call_args, 0, func);
  PyObject *const argspec = PyObject_CallObject(getfullargspec_function, argspec_call_args);
  Py_DECREF(argspec_call_args);
  Py_DECREF(getfullargspec_function);

  // Get args from argspec.
  PyObject *const          f_args   = PyObject_GetAttrString(argspec, "args");
  Py_ssize_t const         num_args = PyList_Size(f_args);
  std::vector<std::string> arg_names;
  for (Py_ssize_t i = 0; i < num_args; ++i) {
    PyObject *const arg      = PyList_GetItem(f_args, i);
    PyObject *const arg_repr = PyObject_Repr(arg);
    PyObject *const arg_str  = PyUnicode_AsASCIIString(arg_repr);
    arg_names.emplace_back(PyBytes_AS_STRING(arg_str));
    Py_DECREF(arg);
    Py_DECREF(arg_repr);
    Py_DECREF(arg_str);
  }
  Py_DECREF(f_args);
  return arg_names;
}
} // namespace

PythonAction::PythonAction(
    Timing               timing,
    std::string          modulePath,
    std::string          moduleName,
    const mesh::PtrMesh &mesh,
    int                  targetDataID,
    int                  sourceDataID)
    : Action(timing, mesh),
      _modulePath(std::move(modulePath)),
      _moduleName(std::move(moduleName))
{
  PRECICE_CHECK(boost::filesystem::is_directory(_modulePath),
                "The module path of the python action \"{}\" does not exist. The configured path is \"{}\".",
                _moduleName, _modulePath);
  if (targetDataID != -1) {
    _targetData = getMesh()->data(targetDataID);
    _numberArguments++;
  }
  if (sourceDataID != -1) {
    _sourceData = getMesh()->data(sourceDataID);
    _numberArguments++;
  }
}

PythonAction::~PythonAction()
{
  if (_module != nullptr) {
    PRECICE_ASSERT(_moduleNameObject != nullptr);
    PRECICE_ASSERT(_module != nullptr);
    Py_DECREF(_moduleNameObject);
    Py_DECREF(_module);
    Py_Finalize();
  }
}

void PythonAction::performAction(double time,
                                 double timeStepSize,
                                 double computedTimeWindowPart,
                                 double timeWindowSize)
{
  PRECICE_TRACE(time, timeStepSize, computedTimeWindowPart, timeWindowSize);

  if (not _isInitialized)
    initialize();

  PyObject *dataArgs = PyTuple_New(_numberArguments);
  if (_performAction != nullptr) {
    PyObject *pythonTime           = PyFloat_FromDouble(time);
    PyObject *pythonTimeWindowSize = PyFloat_FromDouble(timeWindowSize);
    PyTuple_SetItem(dataArgs, 0, pythonTime);
    PyTuple_SetItem(dataArgs, 1, pythonTimeWindowSize);
    if (_sourceData) {
      npy_intp sourceDim[]  = {_sourceData->values().size()};
      double * sourceValues = _sourceData->values().data();
      //PRECICE_ASSERT(_sourceValues == NULL);
      _sourceValues = PyArray_SimpleNewFromData(1, sourceDim, NPY_DOUBLE, sourceValues);
      PRECICE_CHECK(_sourceValues != nullptr, "Creating python source values failed. Please check that the source data name is used by the mesh in action:python.");
      PyTuple_SetItem(dataArgs, 2, _sourceValues);
    }
    if (_targetData) {
      npy_intp targetDim[]  = {_targetData->values().size()};
      double * targetValues = _targetData->values().data();
      //PRECICE_ASSERT(_targetValues == NULL);
      _targetValues =
          PyArray_SimpleNewFromData(1, targetDim, NPY_DOUBLE, targetValues);
      PRECICE_CHECK(_targetValues != nullptr, "Creating python target values failed. Please check that the target data name is used by the mesh in action:python.");
      int argumentIndex = _sourceData ? 3 : 2;
      PyTuple_SetItem(dataArgs, argumentIndex, _targetValues);
    }
    PyObject_CallObject(_performAction, dataArgs);
    if (PyErr_Occurred()) {
      PRECICE_ERROR("Error occurred during call of function performAction() in python module \"{}\". "
                    "The error message is: {}",
                    _moduleName, python_error_as_string());
    }
  }

  if (_vertexCallback != nullptr) {
    // The arguments is a tuple of (id, coord) or (id, coord, normal).
    // The deprecated normal is optional and None will be passed if it was defined.
    PRECICE_ASSERT(_vertexCallbackArgs == 2 || _vertexCallbackArgs == 3, _vertexCallbackArgs);
    PyObject *vertexArgs = PyTuple_New(_vertexCallbackArgs);
    if (_vertexCallbackArgs == 3) {
      PyTuple_SetItem(vertexArgs, 2, Py_None);
    }
    mesh::PtrMesh   mesh = getMesh();
    Eigen::VectorXd coords(mesh->getDimensions());
    for (mesh::Vertex &vertex : mesh->vertices()) {
      npy_intp vdim[]        = {mesh->getDimensions()};
      int      id            = vertex.getID();
      coords                 = vertex.getCoords();
      PyObject *pythonID     = PyLong_FromLong(id);
      PyObject *pythonCoords = PyArray_SimpleNewFromData(1, vdim, NPY_DOUBLE, coords.data());
      PRECICE_CHECK(pythonID != nullptr, "Creating python ID failed. Please check that the python-actions mesh name is correct.");
      PRECICE_CHECK(pythonCoords != nullptr, "Creating python coords failed. Please check that the python-actions mesh name is correct.");
      PyTuple_SetItem(vertexArgs, 0, pythonID);
      PyTuple_SetItem(vertexArgs, 1, pythonCoords);
      PyObject_CallObject(_vertexCallback, vertexArgs);
      if (PyErr_Occurred()) {
        PRECICE_ERROR("Error occurred during call of function vertexCallback() in python module \"{}\". "
                      "The error message is: {}",
                      _moduleName, python_error_as_string());
      }
    }
    Py_DECREF(vertexArgs);
  }

  if (_postAction != nullptr) {
    PyObject *postActionArgs = PyTuple_New(0);
    PyObject_CallObject(_postAction, postActionArgs);
    if (PyErr_Occurred()) {
      PRECICE_ERROR("Error occurred during call of function postAction() in python module \"{}\". "
                    "The error message is: {}",
                    _moduleName, python_error_as_string());
    }
    Py_DECREF(postActionArgs);
  }

  Py_DECREF(dataArgs);
}

void PythonAction::initialize()
{
  PRECICE_ASSERT(not _isInitialized);
  // Initialize Python
  Py_Initialize();
  makeNumPyArraysAvailable();
  // Append execution path to find module to import
  PyRun_SimpleString("import sys");
  std::string appendPathCommand("sys.path.append('" + _modulePath + "')");
  PyRun_SimpleString(appendPathCommand.c_str());
  _moduleNameObject = PyUnicode_FromString(_moduleName.c_str());
  _module           = PyImport_Import(_moduleNameObject);
  if (_module == nullptr) {
    PRECICE_ERROR("An error occurred while loading python module \"{}\": {}", _moduleName, python_error_as_string());
  }

  // Construct method performAction
  _performAction = PyObject_GetAttrString(_module, "performAction");
  if (PyErr_Occurred()) {
    PyErr_Clear();
    PRECICE_WARN("Python module \"{}\" does not define function performAction().", _moduleName);
    _performAction = nullptr;
  }
  //  bool valid = _performAction != NULL;
  //  if (valid) valid = PyCallable_Check(_performAction);
  //  if (not valid){
  //  }

  // Construct method vertexCallback
  _vertexCallback = PyObject_GetAttrString(_module, "vertexCallback");
  if (PyErr_Occurred()) {
    PyErr_Clear();
    PRECICE_WARN("Python module \"{}\" does not define function vertexCallback().", _moduleName);
    _vertexCallback = nullptr;
  } else {
    _vertexCallbackArgs = python_func_args(_vertexCallback).size();
    if (_vertexCallbackArgs == 3) {
      PRECICE_WARN("Python module \"{}\" defines the function vertexCallback with 3 arguments. "
                   "The normal argument is deprecated and preCICE will pass None instead. "
                   "Please use the following definition to silence this warning \"def vertexCallback(id, coords):\".",
                   _moduleName);
    }
    PRECICE_CHECK(_vertexCallbackArgs == 2 || _vertexCallbackArgs == 3,
                  "The provided vertexCallback() in python module \"{}\" has {} arguments, but needs to have 2 or 3. "
                  "Please use the following definition \"def vertexCallback(id, coords):\"",
                  _moduleName, _vertexCallbackArgs);
  }

  // Construct function postAction
  _postAction = PyObject_GetAttrString(_module, "postAction");
  if (PyErr_Occurred()) {
    PyErr_Clear();
    PRECICE_WARN("Python module \"{}\" does not define function postAction().", _moduleName);
    _postAction = nullptr;
  }
}

int PythonAction::makeNumPyArraysAvailable()
{
  static bool importedAlready = false;
  if (importedAlready)
    return 0;
  import_array1(-1); // this macro is defined be NumPy and must be included
  importedAlready = true;
  return 1;
}

} // namespace precice::action

#endif

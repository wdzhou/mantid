#include "MantidAPI/ITableWorkspace.h"
#include "MantidAPI/Column.h"
#include "MantidAPI/TableRow.h"
#include "MantidAPI/WorkspaceProperty.h"
#include "MantidPythonInterface/kernel/SharedPtrToPythonMacro.h"
#include "MantidPythonInterface/kernel/Registry/RegisterSingleValueHandler.h"
#include "MantidPythonInterface/kernel/PropertyWithValue.h"

#include <boost/python/class.hpp>
#include <boost/python/list.hpp>
#include <boost/python/dict.hpp>
#include <boost/python/converter/builtin_converters.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/tuple/to_list.hpp>
#include <vector>

using Mantid::API::ITableWorkspace;
using Mantid::API::ITableWorkspace_sptr;
using Mantid::API::TableRow;
using Mantid::API::Column_sptr;
using Mantid::API::Workspace;
using Mantid::Kernel::DataItem_sptr;
using namespace boost::python;

namespace
{
  namespace bpl = boost::python;

  /// Boost macro for "looping" over builtin types
  #define BUILTIN_TYPES \
    BOOST_PP_TUPLE_TO_LIST( \
        6, (double, std::string, int, int64_t, bool, float) \
    )
  #define USER_TYPES \
    BOOST_PP_TUPLE_TO_LIST( \
        1, (Mantid::Kernel::V3D) \
    )

  /**
   * Get out the Python value from a specific cell of the supplied column. This is meant to
   * reduce the amount of copy and pasted code in this file.
   * @param column The column to grab the value from.
   * @param typeID The python identifier of the column type.
   * @param row The row to get the value from.
   */
  PyObject *getValue(Mantid::API::Column_const_sptr column, const std::type_info & typeID, const int row)
  {
    #define GET_BUILTIN(R, _, T) \
    else if(typeID == typeid(T)) \
    {\
      result = to_python_value<const T&>()(column->cell<T>(row));\
    }
    #define GET_USER(R, _, T) \
    else if(typeID == typeid(T))\
    {\
      const converter::registration *entry = converter::registry::query(typeid(T));\
      if(!entry) throw std::invalid_argument("Cannot find converter from C++ type.");\
      result = entry->to_python((const void *)&column->cell<T>(row));\
    }

    // -- Use the boost preprocessor to generate a list of else if clause to cut out copy
    // and pasted code.
    PyObject *result(NULL);
    if(false){} // So that it always falls through to the list checking
    BOOST_PP_LIST_FOR_EACH(GET_BUILTIN, _ , BUILTIN_TYPES)
    BOOST_PP_LIST_FOR_EACH(GET_USER, _ , USER_TYPES)
    else
    {
      throw std::invalid_argument("Cannot convert C++ type to Python: " + column->type());
    }
    return result;
  }

  /**
   * Sets a value in a particular column and row from a python object
   * @param column :: A pointer to the column object
   * @param row :: The index of the row
   * @param value :: The value to set
   */
  void setValue(const Column_sptr column, const int row, const bpl::object & value)
  {
#define SET_CELL(R, _, T) \
    else if(typeID == typeid(T)) \
    {\
      column->cell<T>(row) = bpl::extract<T>(value)();\
    }
    // -- Use the boost preprocessor to generate a list of else if clause to cut out copy
    // and pasted code.
    const std::type_info & typeID = column->get_type_info();
    if(false){} // So that it always falls through to the list checking
    BOOST_PP_LIST_FOR_EACH(SET_CELL, _ , BUILTIN_TYPES)
    BOOST_PP_LIST_FOR_EACH(SET_CELL, _ , USER_TYPES)
    else
    {
      throw std::invalid_argument("Cannot convert Python type to C++: " + column->type());
    }
  }

  /**
   * Access a cell and return a corresponding Python type
   * @param self A reference to the TableWorkspace python object that we were called on
   * @param value A python object containing either a row index or a column name
   * @param row_or_col An integer giving the row if value is a string or the column if value is an index
   */
  PyObject * cell(ITableWorkspace &self, bpl::object value, int row_or_col)
  {
    // Find the column and row
    Mantid::API::Column_const_sptr column;
    int row(-1);
    if( PyString_Check(value.ptr()) )
    {
      column = self.getColumn( extract<std::string>(value)());
      row = row_or_col;
    }
    else
    {
      row = extract<int>(value)();
      column = self.getColumn(row_or_col);
    }

    const std::type_info & typeID = column->get_type_info();
    return getValue(column, typeID, row);
  }

  /**
   * Access a cell and return a corresponding Python type
   * @param self A reference to the TableWorkspace python object that we were called on
   * @param value A python object containing a column name or index
   */
  PyObject * column(ITableWorkspace &self, bpl::object value)
  {
    // Find the column and row
    Mantid::API::Column_const_sptr column;
    if( PyString_Check(value.ptr()) )
    {
      column = self.getColumn( extract<std::string>(value)());
    }
    else
    {
      column = self.getColumn(extract<int>(value)());
    }
    const std::type_info & typeID = column->get_type_info();
    const int numRows = static_cast<int>(column->size());

    PyObject *result = PyList_New(numRows);
    for (int i = 0; i < numRows; i++)
    {
      if(PyList_SetItem(result, i, getValue(column, typeID, i)))
        throw std::runtime_error("Error while building list");
    }

    return result;
  }

  /**
   * Access a cell and return a corresponding Python type
   * @param self A reference to the TableWorkspace python object that we were called on
   * @param row An integer giving the row
   */
  PyObject * row(ITableWorkspace &self, int row)
  {
    if (row < 0)
      throw std::invalid_argument("Cannot specify negative row number");
    if (row >= static_cast<int>(self.rowCount()))
      throw std::invalid_argument("Cannot specify row larger than number of rows");

    int numCols = static_cast<int>(self.columnCount());

    PyObject *result = PyDict_New();

    for (int col = 0; col < numCols; col++)
    {
      Mantid::API::Column_const_sptr column = self.getColumn(col);
      const std::type_info & typeID = column->get_type_info();

      if (PyDict_SetItemString(result, column->name().c_str(), getValue(column, typeID, row)))
        throw std::runtime_error("Error while building dict");
    }

    return result;
  }

  /**
   * Adds a new row in the table, where the items are given in a
   * dictionary object mapping {column name:value}
   * @param self :: A reference to the ITableWorkspace object
   * @param rowItems :: A dictionary defining the items in the row. It must
   * be the same length as the number of columns or the insert will fail
   */
  void addRowFromDict(ITableWorkspace & self, const bpl::dict & rowItems)
  {
    bpl::ssize_t nitems = boost::python::len(rowItems);
    if( nitems != static_cast<bpl::ssize_t>(self.columnCount()) )
    {
      throw std::invalid_argument("Number of values given does not match the number of columns.");
    }
    const int rowIndex = static_cast<int>(self.rowCount());
    TableRow newRow = self.appendRow();
    boost::python::object iter = rowItems.iteritems();
    while(true)
    {
      bpl::object keyValue;
      try
      {
        keyValue = iter.attr("next")();
      }
      catch(bpl::error_already_set&)
      {
        PyErr_Clear(); // Clear the raised StopIteration exception
        break;
      }
      const std::string columnName = boost::python::extract<std::string>(keyValue[0]);
      const Column_sptr column = self.getColumn(columnName);
      try
      {
        setValue(column, rowIndex, keyValue[1]);
      }
      catch(bpl::error_already_set&)
      {
        std::ostringstream os;
        os << "Incorrect type passed for \"" << columnName << "\"";
        throw std::invalid_argument(os.str());
      }
    }
  }

  /**
   * Adds a new row in the table, where the items are given in a
   * dictionary object mapping {column name:value}
   * @param self :: A reference to the ITableWorkspace object
   * @param rowItems :: A dictionary defining the items in the row. It must
   * be the same length as the number of columns or the insert will fail
   */
  void addRowFromList(ITableWorkspace & self, const bpl::list & rowItems)
  {
    bpl::ssize_t nitems = boost::python::len(rowItems);
    if( nitems != static_cast<bpl::ssize_t>(self.columnCount()) )
    {
      throw std::invalid_argument("Number of values given does not match the number of columns.");
    }
    const int rowIndex = static_cast<int>(self.rowCount());
    TableRow newRow = self.appendRow();
    for(bpl::ssize_t i = 0; i < nitems; ++i)
    {
      const Column_sptr column = self.getColumn(i);
      try
      {
        setValue(column, rowIndex, rowItems[i]);
      }
      catch(bpl::error_already_set&)
      {
        std::ostringstream os;
        os << "Incorrect type passed for item \"" << i << "\" in list";
        throw std::invalid_argument(os.str());
      }
    }
  }

}

void export_ITableWorkspace()
{
  REGISTER_SHARED_PTR_TO_PYTHON(ITableWorkspace);
  std::string iTableWorkspace_docstring = "Most of the information from a table workspace is returned ";
  iTableWorkspace_docstring += "as native copies. All of the column accessors return lists while the ";
  iTableWorkspace_docstring += "rows return dicts. This object does support the idom 'for row in ";
  iTableWorkspace_docstring += "ITableWorkspace'.";

  class_<ITableWorkspace,bases<Workspace>, boost::noncopyable>("ITableWorkspace",
         iTableWorkspace_docstring.c_str(),
         no_init)
    .def("addColumn", &ITableWorkspace::addColumn, (arg("type"), arg("name")),
         "Add a named column with the given type. Recognized types are: int,float,double,bool,str,V3D,long64")

    .def("removeColumn", &ITableWorkspace::removeColumn, (arg("name")),
         "Remove the named column")

    .def("columnCount", &ITableWorkspace::columnCount,
         "Returns the number of columns in the workspace")

    .def("rowCount", &ITableWorkspace::rowCount,
         "Returns the number of rows within the workspace")

    .def("setRowCount", &ITableWorkspace::setRowCount, (arg("count")),
         "Resize the table to contain count rows")

    .def("__len__",  &ITableWorkspace::rowCount, "Returns the number of rows within the workspace")

    .def("getColumnNames",&ITableWorkspace::getColumnNames, "Return a list of the column names")

    .def("keys", &ITableWorkspace::getColumnNames,  "Return a list of the column names")

    .def("column", &column, "Return all values of a specific column as a list")

    .def("row", &row,
         "Return all values of a specific row as a dict")

    .def("addRow", &addRowFromDict,
         "Appends a row with the values from the dictionary")

    .def("addRow", &addRowFromList, "Appends a row with the values from the given list. "
         "It it assumed that the items are in the correct order for the defined columns")

    .def("cell", &cell, "Return the given cell. If the first argument is a "
         "number then it is interpreted as a row otherwise it is interpreted as a column name")
      ;

  REGISTER_SINGLEVALUE_HANDLER(ITableWorkspace_sptr);
}


Change Log for libSplash
================================================================

Release 1.1.1
-------------
**Date:** 2014-02-20

**Bug Fixes**

 - Allow to write/read attributes at groups #99


Release 1.1.0
-------------
**Date:** 2014-01-23

**New Features**

 - Added support for various new collection types.
 - Added macros to generate array and compound collection types.
 - Enabled support for using static HDF5 library.
 - Added patch level and file format version information.
 - Updated user manual for parallel I/O.
 - New tests and examples.


**Interface Changes**

 - Header files moved to splash subfolder (include/splash/splash.h). 


**Bug Fixes**

 - Create duplicate of user-provided MPI communicator.
 - Allow empty write/read calls (no data).
 - Improved HDF5 detection in cmake file.
 - Fix usage of MPI data types.


**Misc:**

 - Compiling with OpenMPI now requires version 1.5.1 or higher
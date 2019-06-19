# arguments
runlink                = static
platform               = linux/x64



# paths home
project_home           = ..
build_dir              = $(project_home)/sln
bin_dir                = $(project_home)/lib/$(platform)
object_dir             = $(project_home)/.objs



# includes of cm256 headers
cm256_inc_path         = $(project_home)/gnu/inc
cm256_includes         = -I$(cm256_inc_path)

# includes of cauchy_fec headers
cauchy_fec_inc_path    = $(project_home)/inc
cauchy_fec_includes    = -I$(cauchy_fec_inc_path)



# all includes that cauchy_fec solution needs
includes               = $(cm256_includes)
includes              += $(cauchy_fec_includes)



# source files of cm256 solution
cm256_src_path         = $(project_home)/gnu/src
cm256_source           = $(filter %.cpp, $(shell find $(cm256_src_path) -depth -name "*.cpp"))

# source files of cauchy_fec solution
cauchy_fec_src_path    = $(project_home)/src
cauchy_fec_source      = $(filter %.cpp, $(shell find $(cauchy_fec_src_path) -depth -name "*.cpp"))



# objects of cauchy_fec solution
cauchy_fec_objects     = $(cm256_source:$(project_home)%.cpp=$(object_dir)%.o)
cauchy_fec_objects    += $(cauchy_fec_source:$(project_home)%.cpp=$(object_dir)%.o)



# cauchy_fec depends libraries
cauchy_fec_depends     =



# output libraries
ifeq ($(runlink), static)
	cauchy_fec_outputs = $(bin_dir)/libcauchy_fec.a
else ifeq ($(platform), mac)
	cauchy_fec_outputs = $(bin_dir)/libcauchy_fec.dylib
else
	cauchy_fec_outputs = $(bin_dir)/libcauchy_fec.so
endif



# build output command line
ifeq ($(runlink), static)
	build_command      = ar -rv $(cauchy_fec_outputs) $^
else
	build_command      = g++ -std=c++11 -shared -o $(cauchy_fec_outputs) $^ $(cauchy_fec_depends)
endif



# build targets
targets                = cauchy_fec

# let 'build' be default target, build all targets
build    : $(targets)

cauchy_fec      : $(cauchy_fec_objects)
	@echo
	@echo "@@@@@  start making cauchy_fec  @@@@@"
	$(build_command)
	@echo "@@@@@  make cauchy_fec success  @@@@@"
	@echo

# build all objects
$(object_dir)/%.o:$(project_home)/%.cpp
	@dir=`dirname $@`;		\
	if [ ! -d $$dir ]; then	\
		mkdir -p $$dir;		\
	fi
	g++ -c -std=c++11 -g -Wall -O1 -pipe -fPIC -mssse3 -DUSE_SSSE3 $(includes) -o $@ $<

clean            :
	rm -rf $(object_dir) $(bin_dir)/libcauchy_fec.*

rebuild          : clean build

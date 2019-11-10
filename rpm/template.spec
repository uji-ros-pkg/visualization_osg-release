Name:           ros-melodic-osg-interactive-markers
Version:        1.0.2
Release:        2%{?dist}
Summary:        ROS osg_interactive_markers package

Group:          Development/Libraries
License:        BSD
Source0:        %{name}-%{version}.tar.gz

Requires:       OpenSceneGraph
Requires:       OpenSceneGraph-devel
Requires:       OpenThreads
Requires:       OpenThreads-devel
Requires:       ros-melodic-interactive-markers
Requires:       ros-melodic-osg-markers
Requires:       ros-melodic-osg-utils
Requires:       ros-melodic-roscpp
Requires:       ros-melodic-tf
Requires:       ros-melodic-visualization-msgs
BuildRequires:  OpenSceneGraph
BuildRequires:  OpenSceneGraph-devel
BuildRequires:  OpenThreads
BuildRequires:  OpenThreads-devel
BuildRequires:  ros-melodic-catkin
BuildRequires:  ros-melodic-interactive-markers
BuildRequires:  ros-melodic-osg-markers
BuildRequires:  ros-melodic-osg-utils
BuildRequires:  ros-melodic-roscpp
BuildRequires:  ros-melodic-tf
BuildRequires:  ros-melodic-visualization-msgs

%description
This package is basically an OpenSceneGraph (OSG) adaptation of the Interactive
Markers client writen for rviz/Ogre.

%prep
%setup -q

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree that was dropped by catkin, and source it.  It will
# set things like CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/melodic/setup.sh" ]; then . "/opt/ros/melodic/setup.sh"; fi
mkdir -p obj-%{_target_platform} && cd obj-%{_target_platform}
%cmake .. \
        -UINCLUDE_INSTALL_DIR \
        -ULIB_INSTALL_DIR \
        -USYSCONF_INSTALL_DIR \
        -USHARE_INSTALL_PREFIX \
        -ULIB_SUFFIX \
        -DCMAKE_INSTALL_LIBDIR="lib" \
        -DCMAKE_INSTALL_PREFIX="/opt/ros/melodic" \
        -DCMAKE_PREFIX_PATH="/opt/ros/melodic" \
        -DSETUPTOOLS_DEB_LAYOUT=OFF \
        -DCATKIN_BUILD_BINARY_PACKAGE="1" \

make %{?_smp_mflags}

%install
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree that was dropped by catkin, and source it.  It will
# set things like CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/melodic/setup.sh" ]; then . "/opt/ros/melodic/setup.sh"; fi
cd obj-%{_target_platform}
make %{?_smp_mflags} install DESTDIR=%{buildroot}

%files
/opt/ros/melodic

%changelog
* Sun Nov 10 2019 Javier Perez <japerez@uji.es> - 1.0.2-2
- Autogenerated by Bloom

* Sat Oct 12 2019 Javier Perez <japerez@uji.es> - 1.0.2-1
- Autogenerated by Bloom


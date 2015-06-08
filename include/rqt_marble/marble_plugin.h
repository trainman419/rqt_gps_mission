// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

/* -- BEGIN LICENSE BLOCK ----------------------------------------------

 Copyright (c) 2013, TB
 All rights reserved.

 Redistribution and use in source and binary forms are permitted
 provided that the above copyright notice and this paragraph are
 duplicated in all such forms and that any documentation,
 advertising materials, and other materials related to such
 distribution and use acknowledge that the software was developed
 by TB.  The name of the
 TB may not be used to endorse or promote products derived
 from this software without specific prior written permission.
 THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

 -- END LICENSE BLOCK ----------------------------------------------*/

//----------------------------------------------------------------------
/*!\file
 *
 * \author  Tobias Bär <baer@fzi.de>
 * \date    2013-01-11
 *
 */
//----------------------------------------------------------------------
#ifndef _MARBLE_PLUGIN_H
#define _MARBLE_PLUGIN_H

#include <QtCore/QObject>
#include <QMutex>
#include <marble/RouteRequest.h>
#include <marble/RoutingManager.h>
#include <marble/MapThemeManager.h>
#include <marble/Route.h>

#include <ros/ros.h>
#include <rqt_gui_cpp/plugin.h>
#include <sensor_msgs/NavSatFix.h>
#include <rqt_marble/RouteGps.h>

#include <ui_marble_plugin.h> // Generated into ./build/rqt_robot_plugins/rqt_marble by Catkin.

namespace rqt_marble
{

/**
 * MarblePlugin class works as an interface for 2 libraries:
 * for Marble via Ui_form class and for rqt framework by extending
 * rqt_gui_cpp::Plugin class.
 */
class MarblePlugin : public rqt_gui_cpp::Plugin
{
Q_OBJECT
public:
  MarblePlugin();
  virtual void initPlugin(qt_gui_cpp::PluginContext& context);
  virtual void shutdownPlugin();
  virtual void saveSettings(qt_gui_cpp::Settings& plugin_settings,
                            qt_gui_cpp::Settings& instance_settings) const;
  virtual void restoreSettings(const qt_gui_cpp::Settings& plugin_settings,
                               const qt_gui_cpp::Settings& instance_settings);

  // Comment in to signal that the plugin has a way to configure it
  //bool hasConfiguration() const;
  //void triggerConfiguration();

  void gpsCallback(const sensor_msgs::NavSatFixConstPtr& gpspt);

private:

Q_SIGNALS:

  void newGpsPosition(qreal, qreal);

private Q_SLOTS:

  void changeGpsTopic(const QString &topic_name);
  void setKmlFile(bool envoke_file_dialog = true);
  /**
   * @param idx: index within combobox
   */
  void changeMarbleModelTheme(int idx);
  void enableNavigation(bool checked);
  void routeChanged();

  /**
   * Capture GPS Topics from ROS and set the topic names on combo box.
   */
  void findGpsTopics();
  void gpsUpdate(qreal, qreal);

private:
  Ui_MarblePluginWidget ui_;
  QWidget* widget_;
  ros::Subscriber m_sat_nav_fix_subscriber;
  Marble::RoutingManager* routing_manager;
  Marble::RouteRequest* request;
  Marble::RoutingModel* routeModel;
  Marble::MapThemeManager* map_theme_manager_;

  void initWidget(qt_gui_cpp::PluginContext& context);

  /* ROS bits */
  ros::NodeHandle nh_;
  ros::Publisher route_pub_;
  bool do_navigation_;

  /**
   * @arg Route route: the route to convert
   * @return: the route as an rqt_marble::RouteGPS message
   */
  rqt_marble::RouteGps marbleRouteToROS(Marble::Route route);

  // for recentering
  qreal x_, y_;
};
} // namespace
#endif // _MARBLE_PLUGIN_H

// this is for emacs file handling -*- mode: c++; indent-tabs-mode: nil -*-

/* -- BEGIN LICENSE BLOCK ----------------------------------------------

 Copyright (c) 2013, TB
 All rights reserved.

 Redistribution and use in source and binary forms are permitted
 provided that the above copyright notice and this paragraph are
 duplicated in all such forms and that any documentation,
 advertising materials, and other materials related to such
 distribution and use acknowledge that the software was developed
 by TB. The name of the
 TB may not be used to endorse or promote products derived
 from this software without specific prior written permission.
 THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

 -- END LICENSE BLOCK ----------------------------------------------*/

//----------------------------------------------------------------------
/*!\file
 *
 * \author  Tobias Bär <baer@fzi.de> Jan Aidel <aiden@fzi.de>
 * \date    2013-01-11
 *
 */
//----------------------------------------------------------------------
#include <marble/MarbleWidget.h>
#include <marble/MarbleModel.h>
#include <marble/MapThemeManager.h>
#include <marble/GeoPainter.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/Route.h>
#include <marble/RoutingModel.h>
// Qt Includes
#include <QLineEdit>
#include <QFileInfo>
#include <QFileDialog>
#include <QStringList>
#include <QStandardItemModel>
#include <QModelIndex>

#include <pluginlib/class_list_macros.h>
#include <ros/package.h>

#include "rqt_marble/marble_plugin.h"

// @TODO: setDistance does not work on reloading
// @TODO: ComboBox for the MarbleWidget projection method
// @TOOD: Draw icon on the current gps pos (MarbleWidget needs to be subclassed (custom paint))

namespace rqt_marble
{

MarblePlugin::MarblePlugin() :
    rqt_gui_cpp::Plugin(),
    widget_(0),
    do_navigation_(false),
    x_(-1), y_(-1)
{
  // give QObjects reasonable names
  setObjectName("MarbleWidgetPlugin");

  route_pub_ = nh_.advertise<RouteGps>("route_gps", 1000);
  ROS_INFO("in constructor");
}

/**
 * Overridden from rqt_gui_cpp::Plugin
 */
void MarblePlugin::initPlugin(qt_gui_cpp::PluginContext& context)
{
  ROS_INFO("in initPlugin");

  initWidget(context);

  this->findGpsTopics();
}

void MarblePlugin::initWidget(qt_gui_cpp::PluginContext& context)
{

  // create QWidget
  widget_ = new QWidget();

  // add widget to the user interface
  ui_.setupUi(widget_);
  ui_.marble_widget->setMapThemeId("earth/openstreetmap/openstreetmap.dgml");
  ui_.marble_widget->setProjection(Marble::Mercator);
  //ui_.marble_widget->centerOn(115.87164, -31.93452, false); // My Happy Place: The Scotto
  ui_.marble_widget->centerOn(-122.0795, 37.4000, false); // OSRF
  ui_.marble_widget->setDistance(0.05);

  context.addWidget(widget_);
  map_theme_manager = new Marble::MapThemeManager(widget_);
  ui_._combobox_theme->setModel(map_theme_manager->mapThemeModel());

  QIcon refresh_icon; //set refresh icon
  std::string path = ros::package::getPath("rqt_marble") + "/etc/refresh.png";
  QString icon_path(path.c_str());
  refresh_icon.addFile(icon_path);
  ui_._gpstopic_refresh_button->setIcon(refresh_icon);

  //Trying to set proportion on splitter; Map should be much larger that the
  // control pane. But not really sure if the values in the argument are
  // appropriate.
  ui_._splitter_h_top->setStretchFactor(0, 20);

  // Connections
  connect(ui_._gpstopic_combobox, SIGNAL(activated (const QString &)), this,
          SLOT (changeGpsTopic(const QString &)));
  connect(ui_._gpstopic_refresh_button, SIGNAL(clicked()), this,
          SLOT(findGpsTopics()));
  connect(this, SIGNAL(newGpsPosition(qreal, qreal)), ui_.marble_widget,
          SLOT(centerOn(qreal, qreal)));
//  connect( ui_.lineEdit_topic , SIGNAL(editingFinished()) , this , SLOT( changeGpsTopic()) );
  connect(ui_.lineEdit_kml, SIGNAL(returnPressed()), this, SLOT(setKmlFile()));
  connect(ui_._combobox_theme, SIGNAL(currentIndexChanged(int)), this,
          SLOT(changeMarbleModelTheme(int)));
  connect(ui_._checkbox_navigation, SIGNAL(clicked(bool)), this,
          SLOT(enableNavigation(bool)));

// AutoNavigation Connections ... soon
//  m_autoNavigation = new Marble::AutoNavigation(ui_.marble_widget->model(), ui_.ui_.marble_widget->viewport(), this);
//
//  connect(m_autoNavigation, SIGNAL( zoomIn( FlyToMode ) ), ui_.marble_widget, SLOT( zoomIn() ));
//  connect(m_autoNavigation, SIGNAL( zoomOut( FlyToMode ) ), ui_.marble_widget, SLOT( zoomOut() ));
//  connect(m_autoNavigation, SIGNAL( centerOn( const GeoDataCoordinates &, bool ) ), ui_.marble_widget,
//          SLOT( centerOn( const GeoDataCoordinates & ) ));
//  connect(ui_.marble_widget, SIGNAL( visibleLatLonAltBoxChanged() ), m_autoNavigation, SLOT( inhibitAutoAdjustments() ));

  //For GPS Topic emission
  routing_manager = ui_.marble_widget->model()->routingManager();
  this->routeModel = routing_manager->routingModel();
  connect(this->routeModel, SIGNAL(currentRouteChanged()), this,
          SLOT(routeChanged()));
}

// CLEANUP: this looks reasonable.
// TODO: is there a library function for this? what does rviz use?
void MarblePlugin::findGpsTopics()
{
  using namespace ros::master;
  std::vector<TopicInfo> topic_infos;
  getTopics(topic_infos);

  ui_._gpstopic_combobox->clear();
  for (std::vector<TopicInfo>::iterator it = topic_infos.begin();
      it != topic_infos.end(); it++)
  {
    TopicInfo topic = (TopicInfo)(*it);
    if (topic.datatype.compare("sensor_msgs/NavSatFix") == 0)
    {
      QString lineEdit_string(topic.name.c_str());
      ui_._gpstopic_combobox->addItem(lineEdit_string);
    }
  }
}

void MarblePlugin::shutdownPlugin()
{
  // unregister all publishers here
  m_sat_nav_fix_subscriber.shutdown();
  // TODO: unregister route publisher
}

// CLEANUP TODO: why bother?
void MarblePlugin::changeMarbleModelTheme(int idx)
{
  QStandardItemModel* model =
      map_theme_manager->mapThemeModel();
  QModelIndex index = model->index(idx, 0);
  QString theme = model->data(index, Qt::UserRole + 1).toString();

  ui_.marble_widget->setMapThemeId(theme);
}

void MarblePlugin::changeGpsTopic(const QString &topic_name)
{
  m_sat_nav_fix_subscriber.shutdown();
  m_sat_nav_fix_subscriber = nh_.subscribe<sensor_msgs::NavSatFix>(
      topic_name.toStdString(), 10, &MarblePlugin::gpsCallback, this);
}

void MarblePlugin::setKmlFile(bool envoke_file_dialog)
{
  QFileInfo fi(ui_.lineEdit_kml->text());

  if (!fi.isFile() && envoke_file_dialog)
  {
    QString fn = QFileDialog::getOpenFileName(0, tr("Open Geo Data File"),
                                              tr(""),
                                              tr("Geo Data Files (*.kml)"));
    fi.setFile(fn);
  }

  if (fi.isFile())
  {
    ui_.marble_widget->model()->addGeoDataFile(fi.absoluteFilePath());

    ui_.lineEdit_kml->setText(fi.absoluteFilePath());
  }
  else
  {
    ui_.lineEdit_kml->setText("");
  }
}

// CLEANUP: passthough to QT signal
void MarblePlugin::gpsCallback(const sensor_msgs::NavSatFixConstPtr& gpspt)
{
  assert(widget_);

  emit gpsUpdate(gpspt->longitude, gpspt->latitude);
}

// CLEANUP: appears to work. not terribly smooth?
void MarblePlugin::gpsUpdate(qreal lon, qreal lat) {

  // Emit newGpsPosition only, if it changes significantly. Has to be somehow
  // related to the zoom
  qreal x;
  qreal y;

  // Recenter if lat long is not on screen
  bool recenter = !ui_.marble_widget->screenCoordinates(lon, lat, x, y);
  recenter |= ui_._checkBox_centering->isChecked();

  // Recenter if lat long within <threshold> pixels away from center
  qreal threshold = 20;
  recenter |= ((x - x_) * (x - x_) + (y - y_) * (y - y_)) > threshold;

  if (recenter)
  {
    emit newGpsPosition(lon, lat);
    ui_.marble_widget->screenCoordinates(lon, lat, x_, y_);
  }
}

void MarblePlugin::saveSettings(qt_gui_cpp::Settings& plugin_settings,
                                qt_gui_cpp::Settings& instance_settings) const
{
  // save intrinsic configuration, usually using:
  QString topic(m_sat_nav_fix_subscriber.getTopic().c_str());
  instance_settings.setValue("rqt_marble_topic", topic);
  instance_settings.setValue(
      "rqt_marble_kml_file",
      ui_.lineEdit_kml->text().replace(".", "___dot_replacement___"));
  instance_settings.setValue("rqt_marble_zoom", ui_.marble_widget->distance());
  instance_settings.setValue("marble_theme_index",
                             ui_._combobox_theme->currentIndex());
  instance_settings.setValue("marble_center",
                             ui_._checkBox_centering->isChecked());
}

void MarblePlugin::restoreSettings(
    const qt_gui_cpp::Settings& plugin_settings,
    const qt_gui_cpp::Settings& instance_settings)
{
  // restore intrinsic configuration, usually using:
  const QString topic = instance_settings.value("rqt_marble_topic").toString();
  changeGpsTopic(topic);

  ui_.lineEdit_kml->setText(
      instance_settings.value("rqt_marble_kml_file", "").toString().replace(
          "___dot_replacement___", "."));
  ui_._combobox_theme->setCurrentIndex(
      instance_settings.value("marble_theme_index", 0).toInt());
  ui_._checkBox_centering->setChecked(
      instance_settings.value("marble_center", true).toBool());

  setKmlFile(false);

  // @TODO: Does not work since the KML loading changes the zoom
  ui_.marble_widget->setDistance(
      instance_settings.value("rqt_marble_zoom", 0.05).toReal());
}

/*bool hasConfiguration() const
 {
 return true;
 }

 void triggerConfiguration()
 {
 // Usually used to open a dialog to offer the user a set of configuration
 }*/

void MarblePlugin::enableNavigation(bool checked)
{
  do_navigation_ = checked;
}

void MarblePlugin::routeChanged()
{
  if(do_navigation_) {
    Marble::Route route = this->routeModel->route();
    rqt_marble::RouteGps ros_route = marbleRouteToROS(route);
    route_pub_.publish(ros_route);
  }
}

rqt_marble::RouteGps MarblePlugin::marbleRouteToROS(Marble::Route route)
{
  rqt_marble::RouteGps route_gps;

  ROS_DEBUG("size of route %d", route.size());
  for (int i = 0; i < route.size(); i++) {
    Marble::GeoDataLineString route_segment_line_str = route.at(i).path();
    for (int j = 0; j < route_segment_line_str.size(); j++)
    {
      Marble::GeoDataCoordinates coord = route_segment_line_str.at(j);
      // create GPS msg for ROS
      sensor_msgs::NavSatFix gps_msg;
      gps_msg.latitude = coord.latitude();
      gps_msg.longitude = coord.longitude();
      gps_msg.status.status = sensor_msgs::NavSatStatus::STATUS_FIX;
      gps_msg.status.service = sensor_msgs::NavSatStatus::SERVICE_GPS;

      ROS_INFO("#%dth seg; coord#%dlongi=%f lat=%f", i, j, gps_msg.latitude,
               gps_msg.longitude);

      route_gps.routes.push_back(gps_msg);
    }
  }
  return route_gps;
}

} // namespace

PLUGINLIB_EXPORT_CLASS(rqt_marble::MarblePlugin, rqt_gui_cpp::Plugin)

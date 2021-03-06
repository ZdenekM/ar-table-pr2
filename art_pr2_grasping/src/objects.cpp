// Copyright 2016 Robo@FIT

#include "art_pr2_grasping/objects.h"
#include <string>
#include <vector>

namespace art_pr2_grasping
{
Objects::Objects(boost::shared_ptr<tf::TransformListener> tfl,
                 std::string target_frame)
{
  tfl_ = tfl;
  object_type_srv_ =
      nh_.serviceClient<art_msgs::getObjectType>("/art/db/object_type/get");
  obj_sub_ = nh_.subscribe("/art/object_detector/object_filtered", 1,
                           &Objects::detectedObjectsCallback, this);
  target_frame_ = target_frame;

  visual_tools_.reset(
      new moveit_visual_tools::VisualTools(target_frame_, "markers"));
  visual_tools_->loadPlanningSceneMonitor();
  visual_tools_->setLifetime(10.0);
  visual_tools_->setMuted(false);

  visual_tools_->publishRemoveAllCollisionObjects();
}

void Objects::clear()
{
  boost::recursive_mutex::scoped_lock lock(mutex_);
  visual_tools_->publishRemoveAllCollisionObjects();

  objects_.clear();
  grasped_objects_.clear();
}

bool Objects::isKnownObject(std::string id)
{
  boost::recursive_mutex::scoped_lock lock(mutex_);

  TObjectMap::iterator it = objects_.find(id);
  return it != objects_.end();
}

TObjectInfo Objects::getObject(std::string object_id)
{
  boost::recursive_mutex::scoped_lock lock(mutex_);

  if (isKnownObject(object_id))
    return objects_[object_id];
  else
    throw std::invalid_argument("unknown object_id: " + object_id);
}

bool Objects::transformPose(geometry_msgs::PoseStamped& ps)
{
  try
  {
    if (tfl_->waitForTransform(target_frame_, ps.header.frame_id,
                               ps.header.stamp, ros::Duration(1.0)))
    {
      tfl_->transformPose(target_frame_, ps, ps);
    }
    else
    {
      ROS_ERROR_NAMED("objects", "Transform between %s and %s not available!",
                      target_frame_.c_str(), ps.header.frame_id.c_str());
      return false;
    }
  }
  catch (tf::TransformException& ex)
  {
    ROS_ERROR_NAMED("objects", "%s", ex.what());
    return false;
  }

  return true;
}

void Objects::detectedObjectsCallback(
    const art_msgs::InstancesArrayConstPtr& msg)
{
  boost::recursive_mutex::scoped_lock lock(mutex_);

  // remove outdated objects
  TObjectMap::iterator it;
  std::vector<std::string> ids_to_remove;
  for (it = objects_.begin(); it != objects_.end(); ++it)
  {
    bool found = false;

    for (int i = 0; i < msg->instances.size(); i++)
    {
      if (msg->instances[i].object_id == it->first)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      ids_to_remove.push_back(it->first);

      if (!isGrasped(it->first))
        visual_tools_->cleanupCO(it->first);
    }
  }

  for (int i = 0; i < ids_to_remove.size(); i++)
  {
    TObjectMap::iterator it;
    it = objects_.find(ids_to_remove[i]);
    objects_.erase(it);
  }

  // add and publish currently detected objects
  for (int i = 0; i < msg->instances.size(); i++)
  {
    geometry_msgs::PoseStamped ps;

    ps.header = msg->header;
    ps.pose = msg->instances[i].pose;

    if (!transformPose(ps))
    {
      ROS_WARN_NAMED("objects", "Failed to transform object.");
      continue;
    }

    if (isKnownObject(msg->instances[i].object_id))
    {
      objects_[msg->instances[i].object_id].pose = ps;
    }
    else
    {
      art_msgs::getObjectType srv;
      srv.request.name = msg->instances[i].object_type;

      if (!object_type_srv_.call(srv))
      {
        ROS_ERROR_NAMED("objects", "Failed to call object_type service");
        continue;
      }

      TObjectInfo obj;
      obj.object_id = msg->instances[i].object_id;
      obj.pose = ps;
      objects_[msg->instances[i].object_id].type = srv.response.object_type;
    }
  }

  for (TObjectMap::iterator it = objects_.begin(); it != objects_.end(); ++it)
  {
    // do not publish for grasped objects
    if (isGrasped(it->first))
      continue;

    publishObject(it->first);
  }
}

void Objects::publishObject(std::string object_id)
{
    ROS_DEBUG_NAMED("objects", "Publishing object_id: %s", object_id.c_str());

    moveit_msgs::CollisionObject collision_obj;

    collision_obj.header.stamp = ros::Time::now();
    collision_obj.header.frame_id = target_frame_;
    collision_obj.id = object_id;
    collision_obj.operation =
        moveit_msgs::CollisionObject::ADD;  // TODO(ZdenekM): param for update?
    collision_obj.primitives.resize(1);
    collision_obj.primitives[0] = objects_[object_id].type.bbox;
    collision_obj.primitive_poses.resize(1);
    collision_obj.primitive_poses[0] = objects_[object_id].pose.pose;

    for (int j = 0; j < 3; j++)
    {
      visual_tools_->publishCollisionObjectMsg(collision_obj);
      // ros::Duration(0.1).sleep();
    }

    visual_tools_->publishBlock(objects_[object_id].pose.pose, moveit_visual_tools::BLUE,
                                objects_[object_id].type.bbox.dimensions[0],
                                objects_[object_id].type.bbox.dimensions[1],
                                objects_[object_id].type.bbox.dimensions[2]);
}

void Objects::setGrasped(std::string object_id, bool grasped)
{
  boost::recursive_mutex::scoped_lock lock(mutex_);

  if (grasped)
  {
    ROS_DEBUG_NAMED("objects", "Setting object_id as grasped: %s", object_id.c_str());
    grasped_objects_.insert(object_id);
  }
  else
  {
    ROS_DEBUG_NAMED("objects", "Setting object_id as not grasped: %s", object_id.c_str());
    grasped_objects_.erase(object_id);

    if (!isKnownObject(object_id))
    {
      visual_tools_->cleanupCO(object_id);
    }
    else
    {
      publishObject(object_id);
    }
  }
}

}  // namespace art_pr2_grasping

/*
 * Copyright (c) 2011, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * - May 2012: Port to OpenSceneGraph. Mario Prats
 */

#include "osg_interactive_markers/interactive_marker_display.h"
#include "osg_interactive_markers/interactive_marker_client.h"

#include <osg_utils/frame_manager.h>

#include <tf/transform_listener.h>

namespace osg_interactive_markers
{

//////////////
bool validateFloats(const visualization_msgs::InteractiveMarker& msg)
{
	bool valid = true;
	//TODO
	//valid = valid && validateFloats(msg.pose);
	//valid = valid && validateFloats(msg.scale);
	//for ( unsigned c=0; c<msg.controls.size(); c++)
	//{
	//valid = valid && validateFloats( msg.controls[c].orientation );
	//for ( unsigned m=0; m<msg.controls[c].markers.size(); m++ )
	//{
	//valid = valid && validateFloats(msg.controls[c].markers[m].pose);
	//valid = valid && validateFloats(msg.controls[c].markers[m].scale);
	//valid = valid && validateFloats(msg.controls[c].markers[m].color);
	//valid = valid && validateFloats(msg.controls[c].markers[m].points);
	//}
	//}
	return valid;
}
/////////////



InteractiveMarkerDisplay::InteractiveMarkerDisplay( const std::string& name, const std::string& update_topic, osg::Node* root, tf::TransformListener &tf_listener)
: im_client_( this )
, tf_filter_(tf_listener, "", 100, update_nh_)
, tf_pose_filter_(tf_listener, "", 100, update_nh_)
, show_descriptions_(true)
, show_tool_tips_(true)
, show_axes_(false)
{
	scene_node_ = root;

	//message flow: incomingMarkerUpdate(..) -> tf_filter_ -> tfMarkerSuccess(..) -> message_queue_ -> update(..)

	tf_filter_.registerCallback(boost::bind(&InteractiveMarkerDisplay::tfMarkerSuccess, this, _1));
	tf_filter_.registerFailureCallback(boost::bind(&InteractiveMarkerDisplay::tfMarkerFail, this, _1, _2));
	tf_pose_filter_.registerCallback(boost::bind(&InteractiveMarkerDisplay::tfPoseSuccess, this, _1));
	tf_pose_filter_.registerFailureCallback(boost::bind(&InteractiveMarkerDisplay::tfPoseFail, this, _1, _2));

	client_id_ = ros::this_node::getName() + "/" + name;
	setMarkerUpdateTopic(update_topic);
}

InteractiveMarkerDisplay::~InteractiveMarkerDisplay()
{
	unsubscribe();
}

void InteractiveMarkerDisplay::onEnable()
{
	subscribe();
}

void InteractiveMarkerDisplay::onDisable()
{
	unsubscribe();
	tf_filter_.clear();
	tf_pose_filter_.clear();
}

void InteractiveMarkerDisplay::setMarkerUpdateTopic(const std::string& topic)
{
	if ( marker_update_topic_.empty() && topic.empty() )
	{
		return;
	}
	unsubscribe();
	marker_update_topic_ = topic;
	subscribe();
}

void InteractiveMarkerDisplay::subscribe()
{
	marker_update_sub_.shutdown();
	num_publishers_ = 0;

	try
	{
		if ( !marker_update_topic_.empty() )
		{
			ROS_DEBUG( "Subscribing to %s", marker_update_topic_.c_str() );
			marker_update_sub_ = update_nh_.subscribe(marker_update_topic_, 100, &InteractiveMarkerClient::processMarkerUpdate, &im_client_);
		}
	}
	catch (ros::Exception& e)
	{
		ROS_WARN_STREAM("InteractiveMarkerDisplay::subscribe Error subscribing: " << e.what());
	}

	im_client_.clear();
}

bool InteractiveMarkerDisplay::subscribeToInit()
{
	bool result = false;

	try
	{
		if ( !marker_update_topic_.empty() )
		{
			std::string init_topic = marker_update_topic_ + "_full";
			ROS_DEBUG( "Subscribing to %s", init_topic.c_str() );
			marker_init_sub_ = update_nh_.subscribe(init_topic, 100, &InteractiveMarkerClient::processMarkerInit, &im_client_);
			result = true;
		}
	}
	catch (ros::Exception& e)
	{
		ROS_WARN_STREAM("Error subscribing: " << e.what());
	}
	return result;
}

void InteractiveMarkerDisplay::clearMarkers()
{
	interactive_markers_.clear();
	tf_filter_.clear();
	tf_pose_filter_.clear();
}

void InteractiveMarkerDisplay::unsubscribe()
{
	marker_update_sub_.shutdown();
	marker_init_sub_.shutdown();
	clearMarkers();
	im_client_.unsubscribedFromInit();
}

void InteractiveMarkerDisplay::unsubscribeFromInit()
{
	marker_init_sub_.shutdown();
}

void InteractiveMarkerDisplay::processMarkerChanges( const std::vector<visualization_msgs::InteractiveMarker>* markers,
		const std::vector<visualization_msgs::InteractiveMarkerPose>* poses,
		const std::vector<std::string>* erases )
{
	std::set<std::string> names;

	if( markers != NULL )
	{
		std::vector<visualization_msgs::InteractiveMarker>::const_iterator marker_it = markers->begin();
		std::vector<visualization_msgs::InteractiveMarker>::const_iterator marker_end = markers->end();
		for (; marker_it != marker_end; ++marker_it)
		{
			if ( !names.insert( marker_it->name ).second )
			{
				ROS_WARN_STREAM("InteractiveMarkerDisplay::processMarkerChanges: The name '" << marker_it->name << "' was used multiple times.");
			}

			visualization_msgs::InteractiveMarker::ConstPtr marker_ptr(new visualization_msgs::InteractiveMarker(*marker_it));

			if ( marker_it->header.stamp == ros::Time(0) )
			{
				// bypass tf filter
				updateMarker( marker_ptr );
			}
			else
			{
				ROS_DEBUG("Forwarding %s to tf filter.", marker_it->name.c_str());
				tf_filter_.add( marker_ptr );
			}
		}
	}

	if( poses != NULL )
	{
		// pipe all pose updates into tf filter
		std::vector<visualization_msgs::InteractiveMarkerPose>::const_iterator pose_it = poses->begin();
		std::vector<visualization_msgs::InteractiveMarkerPose>::const_iterator pose_end = poses->end();

		for (; pose_it != pose_end; ++pose_it)
		{
			if ( !names.insert( pose_it->name ).second )
			{
				ROS_WARN_STREAM("InteractiveMarkerDisplay::processMarkerChanges: The name '" << pose_it->name << "' was used multiple times.");
			}

			visualization_msgs::InteractiveMarkerPose::ConstPtr pose_ptr(new visualization_msgs::InteractiveMarkerPose(*pose_it));

			if ( pose_it->header.stamp == ros::Time(0) )
			{
				updatePose( pose_ptr );
			}
			else
			{
				ROS_DEBUG("Forwarding pose for %s to tf filter.", pose_it->name.c_str());
				tf_pose_filter_.add( pose_ptr );
			}
		}
	}

	if( erases != NULL )
	{
		// erase markers
		std::vector<std::string>::const_iterator erase_it = erases->begin();
		std::vector<std::string>::const_iterator erase_end = erases->end();

		for (; erase_it != erase_end; ++erase_it)
		{
			interactive_markers_.erase( *erase_it );
		}
	}
}

void InteractiveMarkerDisplay::tfMarkerSuccess( const visualization_msgs::InteractiveMarker::ConstPtr& marker )
{
	ROS_DEBUG("Queueing %s", marker->name.c_str());
	boost::mutex::scoped_lock lock(queue_mutex_);
	marker_queue_.push_back(marker);
}

void InteractiveMarkerDisplay::tfMarkerFail(const visualization_msgs::InteractiveMarker::ConstPtr& marker, tf::FilterFailureReason reason)
{
}

void InteractiveMarkerDisplay::tfPoseSuccess(const visualization_msgs::InteractiveMarkerPose::ConstPtr& marker_pose)
{
	ROS_DEBUG("Queueing pose for %s", marker_pose->name.c_str());

	boost::mutex::scoped_lock lock(queue_mutex_);
	pose_queue_.push_back(marker_pose);
}

void InteractiveMarkerDisplay::tfPoseFail(const visualization_msgs::InteractiveMarkerPose::ConstPtr& marker_pose, tf::FilterFailureReason reason)
{
}


void InteractiveMarkerDisplay::update(float wall_dt, float ros_dt)
{
	//detect if all servers have shut down
	if ( !im_client_.isPublisherListEmpty() )
	{
		// weak detection of server shutdown
		unsigned num_pub = marker_update_sub_.getNumPublishers();
		if ( num_pub < num_publishers_ )
		{
			reset();
		}
		else
		{
			num_publishers_ = num_pub;
		}

		im_client_.flagLateConnections();
	}

	V_InteractiveMarkerMessage local_marker_queue;
	V_InteractiveMarkerPoseMessage local_pose_queue;

	// the queue is accessed from another thread, so we need to lock it
	// and swap it's contents to a copy
	{
		boost::mutex::scoped_lock lock(queue_mutex_);
		local_marker_queue.swap( marker_queue_ );
		local_pose_queue.swap( pose_queue_ );
	}

	if ( !local_marker_queue.empty() )
	{
		V_InteractiveMarkerMessage::iterator message_it = local_marker_queue.begin();
		V_InteractiveMarkerMessage::iterator message_end = local_marker_queue.end();
		for ( ; message_it != message_end; ++message_it )
		{
			updateMarker( *message_it );
		}
	}

	if ( !local_pose_queue.empty() )
	{
		V_InteractiveMarkerPoseMessage::iterator message_it = local_pose_queue.begin();
		V_InteractiveMarkerPoseMessage::iterator message_end = local_pose_queue.end();
		for ( ; message_it != message_end; ++message_it )
		{
			updatePose( *message_it );
		}
	}

	M_StringToInteractiveMarkerPtr::iterator it;
	for ( it = interactive_markers_.begin(); it != interactive_markers_.end(); it++ )
	{
		it->second->update( wall_dt );
	}
}

void InteractiveMarkerDisplay::updateMarker( visualization_msgs::InteractiveMarker::ConstPtr& marker )
{
	if ( !validateFloats( *marker ) )
	{
		//setStatus( status_levels::Error, marker->name, "Message contains invalid floats!" );
		return;
	}
	ROS_DEBUG("Processing interactive marker '%s'. %d", marker->name.c_str(), (int)marker->controls.size() );

	std::map< std::string, InteractiveMarkerPtr >::iterator int_marker_entry = interactive_markers_.find( marker->name );

	std::string topic = marker_update_topic_;

	topic = ros::names::clean( topic );
	topic = topic.substr( 0, topic.find_last_of( '/' ) );

	if ( int_marker_entry == interactive_markers_.end() )
	{
		int_marker_entry = interactive_markers_.insert( std::make_pair(marker->name, InteractiveMarkerPtr ( new InteractiveMarker(this, scene_node_, topic, client_id_) ) ) ).first;
	}

	if ( int_marker_entry->second->processMessage( marker ) )
	{
		//int_marker_entry->second->setShowAxes(show_axes_);
		int_marker_entry->second->setShowDescription(show_descriptions_);
	}
}

void InteractiveMarkerDisplay::updatePose( visualization_msgs::InteractiveMarkerPose::ConstPtr& marker_pose )
{
	//if ( !validateFloats( marker_pose->pose ) )
	//{
	////setStatus( status_levels::Error, marker_pose->name, "Pose message contains invalid floats!" );
	//return;
	//}

	std::map< std::string, InteractiveMarkerPtr >::iterator int_marker_entry = interactive_markers_.find( marker_pose->name );

	if ( int_marker_entry != interactive_markers_.end() )
	{
		int_marker_entry->second->processMessage( marker_pose );
	}
}

void InteractiveMarkerDisplay::targetFrameChanged()
{
}

void InteractiveMarkerDisplay::fixedFrameChanged()
{
	tf_filter_.setTargetFrame(osg_utils::FrameManager::instance()->getFixedFrame());
	tf_pose_filter_.setTargetFrame(osg_utils::FrameManager::instance()->getFixedFrame());
	reset();
}

void InteractiveMarkerDisplay::reset()
{
	ROS_DEBUG("reset");
	unsubscribe();
	subscribe();
}


void InteractiveMarkerDisplay::setShowDescriptions( bool show )
{
	show_descriptions_ = show;
}


void InteractiveMarkerDisplay::setShowToolTips( bool show )
{
	show_tool_tips_ = show;
}


void InteractiveMarkerDisplay::setShowAxes( bool show )
{
	show_axes_ = show;
}

void InteractiveMarkerDisplay::setStatusOk(const std::string& name, const std::string& text)
{
}

void InteractiveMarkerDisplay::setStatusWarn(const std::string& name, const std::string& text)
{
}

void InteractiveMarkerDisplay::setStatusError(const std::string& name, const std::string& text)
{
}


} // namespace osg_interactive_markers

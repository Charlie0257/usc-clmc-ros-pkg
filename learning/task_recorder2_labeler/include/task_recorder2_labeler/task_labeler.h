/*********************************************************************
  Computational Learning and Motor Control Lab
  University of Southern California
  Prof. Stefan Schaal 
 *********************************************************************
  \remarks		...
 
  \file		task_labeler.h

  \author	Peter Pastor
  \date		Jun 17, 2011

 *********************************************************************/

#ifndef TASK_LABELER_H_
#define TASK_LABELER_H_

// system includes
#include <string>
#include <map>
#include <ros/ros.h>
// #include <boost/thread.hpp>

#include <usc_utilities/assert.h>
#include <usc_utilities/services.h>
#include <usc_utilities/param_server.h>

#include <task_recorder2_msgs/DataSample.h>
#include <task_recorder2_msgs/DataSampleLabel.h>
#include <task_recorder2_msgs/Description.h>
#include <task_recorder2_msgs/TaskRecorderSpecification.h>
#include <task_recorder2_msgs/Accumulate.h>

// local includes
#include <task_recorder2_labeler/task_labeler_io.h>

namespace task_recorder2_labeler
{

template<class MessageType>
  class TaskLabeler
  {

  public:

    typedef boost::shared_ptr<MessageType const> MessageTypeConstPtr;

    /*! Constructor
     */
    TaskLabeler(ros::NodeHandle node_handle);
    /*! Destructor
     */
    virtual ~TaskLabeler() {};

    /*!
     * @param label_type
     * @param description
     * @return True on success, otherwise False
     */
    bool getLabelType(int& label_type,
                      const task_recorder2_msgs::Description description,
                      const int default_label_type = task_recorder2_msgs::DataSampleLabel::BINARY_LABEL);

    /*!
     * @param descriptions
     * @param label_type
     * @return True all provided descriptions have a common label type, otherwise False
     */
    bool getLabelType(int& label_type,
                      const std::vector<task_recorder2_msgs::Description>& descriptions,
                      const int default_label_type = task_recorder2_msgs::DataSampleLabel::BINARY_LABEL);

    /*!
     * @param descriptions
     * @return True all provided descriptions have a common label type, otherwise False
     */
    bool getLabelType(const std::vector<task_recorder2_msgs::Description>& descriptions,
                      const int default_label_type = task_recorder2_msgs::DataSampleLabel::BINARY_LABEL)
    {
      int label_type = 0;
      return getLabelType(label_type, descriptions, default_label_type);
    }

    /*!
     * @param description
     * @param label
     * @return True on success, otherwise False
     */
    bool label(const task_recorder2_msgs::Description description,
               const MessageType label);

    /*!
     * @param description
     * @param trial_id
     * @return True on success, otherwise False
     */
    bool getLastTrialId(const task_recorder2_msgs::Description description,
                        int& trial_id)
    {
      return labeler_io_.getLastTrialId(description, trial_id);
    }

    /*!
     * @param description
     * @return True on success, otherwise False
     */
    bool setLastTrialId(task_recorder2_msgs::Description& description)
    {
      return labeler_io_.setLastTrialId(description);
    }

  private:

    TaskLabelerIO<task_recorder2_msgs::DataSampleLabel> labeler_io_;
    std::vector<TaskLabelerIO<task_recorder2_msgs::DataSampleLabel> > labeler_ios_resample_;
    std::vector<TaskLabelerIO<task_recorder2_msgs::DataSampleLabel> > labeler_ios_raw_;

    std::map<std::string, int> description_label_type_map_;
    std::map<std::string, int>::const_iterator description_label_type_map_iterator_;

    bool automatic_accumulation_;
    ros::ServiceClient accumulate_service_;

    // bool readDescriptionLabelTypeMap();
    bool accumulate(const task_recorder2_msgs::Description& description);
    bool readTaskRecorderSpecifications(std::vector<task_recorder2_msgs::TaskRecorderSpecification>& specifications);

  };

template<class MessageType>
  TaskLabeler<MessageType>::TaskLabeler(ros::NodeHandle node_handle) :
    labeler_io_(node_handle)
  {
    ROS_VERIFY(labeler_io_.initialize());
    accumulate_service_ = labeler_io_.node_handle_.serviceClient<task_recorder2_msgs::Accumulate> ("/TaskAccumulator/accumulate");
    ROS_VERIFY(usc_utilities::read(labeler_io_.node_handle_, "automatic_accumulation", automatic_accumulation_));
    if (labeler_io_.write_out_resampled_data_ || labeler_io_.write_out_raw_data_)
    {
      std::vector<task_recorder2_msgs::TaskRecorderSpecification> specifications;
      ROS_VERIFY(readTaskRecorderSpecifications(specifications));
      if (labeler_io_.write_out_resampled_data_)
      {
        for (int i = 0; i < (int)specifications.size(); ++i)
        {
          TaskLabelerIO<task_recorder2_msgs::DataSampleLabel> labeler_io(labeler_io_.node_handle_);
          ROS_VERIFY(labeler_io.initialize(specifications[i].topic_name));
          labeler_ios_resample_.push_back(labeler_io);
        }
      }
      if (labeler_io_.write_out_raw_data_)
      {
        for (int i = 0; i < (int)specifications.size(); ++i)
        {
          TaskLabelerIO<task_recorder2_msgs::DataSampleLabel> labeler_io(labeler_io_.node_handle_);
          ROS_VERIFY(labeler_io.initialize(specifications[i].topic_name));
          labeler_ios_raw_.push_back(labeler_io);
        }
      }
    }
    ROS_VERIFY(task_recorder2_utilities::readDescriptionLabelTypeMap(labeler_io_.node_handle_, description_label_type_map_));
  }

template<class MessageType>
  bool TaskLabeler<MessageType>::getLabelType(int& label_type,
                                              const task_recorder2_msgs::Description description,
                                              const int default_label_type)
  {
    description_label_type_map_iterator_ = description_label_type_map_.find(description.description);
    if(description_label_type_map_iterator_ == description_label_type_map_.end())
    {
      ROS_WARN("Unknown description >%s<, using default label type >%i< instead.", description.description.c_str(), default_label_type);
      label_type = default_label_type;
    }
    else
    {
      label_type = description_label_type_map_iterator_->second;
    }
    return true;
  }

template<class MessageType>
  bool TaskLabeler<MessageType>::getLabelType(int& label_type,
                                              const std::vector<task_recorder2_msgs::Description>& descriptions,
                                              const int default_label_type)
  {
    int previous_label_type = 0;
    bool selected_items_have_same_label_type = true;
    for (int i = 0; i < (int)descriptions.size() && selected_items_have_same_label_type; ++i)
    {
      ROS_DEBUG("Getting label type for >%s<.", descriptions[i].description.c_str());
      if(!getLabelType(label_type, descriptions[i], default_label_type))
      {
        label_type = default_label_type;
      }
      if (i == 0)
      {
        previous_label_type = label_type;
      }
      else
      {
        if (previous_label_type != label_type)
        {
          selected_items_have_same_label_type = false;
        }
      }
    }
    return selected_items_have_same_label_type;
  }

template<class MessageType>
  bool TaskLabeler<MessageType>::label(const task_recorder2_msgs::Description description,
                                       const MessageType label)
  {
    if(labeler_io_.write_out_resampled_data_)
    {
      for (int i = 0; i < (int)labeler_ios_resample_.size(); ++i)
      {
        ROS_VERIFY(labeler_ios_resample_[i].appendResampledLabel(description, label));
      }
    }
    if(labeler_io_.write_out_raw_data_)
    {
      for (int i = 0; i < (int)labeler_ios_raw_.size(); ++i)
      {
        ROS_VERIFY(labeler_ios_raw_[i].appendRawLabel(description, label));
      }
    }
    ROS_VERIFY(labeler_io_.appendLabel(description, label));
    if(automatic_accumulation_)
    {
      ROS_VERIFY(accumulate(description));
    }
    return true;
  }

template<class MessageType>
  bool TaskLabeler<MessageType>::accumulate(const task_recorder2_msgs::Description& description)
  {
    task_recorder2_msgs::Accumulate::Request request;
    request.description = description;
    task_recorder2_msgs::Accumulate::Response response;
    usc_utilities::waitFor(accumulate_service_);
    ROS_VERIFY(accumulate_service_.call(request, response));
    ROS_INFO_STREAM(response.info);
    return true;
  }

template<class MessageType>
bool TaskLabeler<MessageType>::readTaskRecorderSpecifications(std::vector<task_recorder2_msgs::TaskRecorderSpecification>& specifications)
{
  ros::NodeHandle node_handle("/TaskRecorderManager");
  specifications.clear();
  // read the list of description-label_type mapping from the parameter server
  XmlRpc::XmlRpcValue recorder_map;
  if (!node_handle.getParam("task_recorders", recorder_map))
  {
    ROS_ERROR("Couldn't find parameter >%s/task_recorders<", node_handle.getNamespace().c_str());
    return false;
  }
  if (recorder_map.getType() != XmlRpc::XmlRpcValue::TypeArray)
  {
    ROS_ERROR(">%s/task_recorders must be a struct and not of type >%i<.",
              node_handle.getNamespace().c_str(), (int)recorder_map.getType());
    return false;
  }

  for (int i = 0; i < recorder_map.size(); ++i)
  {
    task_recorder2_msgs::TaskRecorderSpecification specification;

    if (!recorder_map[i].hasMember(task_recorder2_msgs::TaskRecorderSpecification::CLASS_NAME))
    {
      ROS_ERROR("Description-LabelType map must have a field \"%s\".", specification.CLASS_NAME.c_str());
      return false;
    }
    std::string class_name = recorder_map[i][task_recorder2_msgs::TaskRecorderSpecification::CLASS_NAME];
    specification.class_name = class_name;

    if (!recorder_map[i].hasMember(task_recorder2_msgs::TaskRecorderSpecification::TOPIC_NAME))
    {
      ROS_ERROR("Description-LabelType map must have a field \"%s\".", task_recorder2_msgs::TaskRecorderSpecification::TOPIC_NAME.c_str());
      return false;
    }
    std::string topic_name = recorder_map[i][task_recorder2_msgs::TaskRecorderSpecification::TOPIC_NAME];
    specification.topic_name = topic_name;

    std::string service_prefix = "";
    if (recorder_map[i].hasMember(task_recorder2_msgs::TaskRecorderSpecification::SERVICE_PREFIX))
    {
      std::string aux = recorder_map[i][task_recorder2_msgs::TaskRecorderSpecification::SERVICE_PREFIX];
      service_prefix = aux;
    }
    specification.service_prefix = service_prefix;

    std::string variable_name_prefix = "";
    if (recorder_map[i].hasMember(task_recorder2_msgs::TaskRecorderSpecification::VARIABLE_NAME_PREFIX))
    {
      std::string aux = recorder_map[i][task_recorder2_msgs::TaskRecorderSpecification::VARIABLE_NAME_PREFIX];
      variable_name_prefix = aux;
    }
    specification.variable_name_prefix = variable_name_prefix;

    specifications.push_back(specification);
    ROS_DEBUG("Class with name >%s< has topic named >%s<, service prefix >%s<, variable name prefix >%s<.",
              class_name.c_str(), topic_name.c_str(), service_prefix.c_str(), variable_name_prefix.c_str());
  }
  return true;
}


}

#endif /* TASK_LABELER_H_ */

#include "dds/DdsDcpsInfrastructureC.h"
#include "dds/DCPS/WaitSet.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/StaticIncludes.h"
#include "MessengerTypeSupportImpl.h"

#include <cstdlib>
#include <iostream>
using namespace std;
using namespace DDS;
using namespace OpenDDS::DCPS;
using namespace Messenger;

class MessengerListener
  : public virtual OpenDDS::DCPS::LocalObject<DDS::DataReaderListener>
{
public:
  MessengerListener(DDS::ReadCondition_ptr rd)
    : rc_(DDS::ReadCondition::_duplicate(rd))
  {}

  virtual void on_requested_deadline_missed(
    DDS::DataReader_ptr /*reader*/,
    const DDS::RequestedDeadlineMissedStatus & /*status*/) {}

  virtual void on_requested_incompatible_qos(
    DDS::DataReader_ptr /*reader*/,
    const DDS::RequestedIncompatibleQosStatus & /*status*/) {}

  virtual void on_liveliness_changed(
    DDS::DataReader_ptr /*reader*/,
    const DDS::LivelinessChangedStatus & /*status*/) {}

  virtual void on_subscription_matched(
    DDS::DataReader_ptr /*reader*/,
    const DDS::SubscriptionMatchedStatus & /*status*/) {}

  virtual void on_sample_rejected(
    DDS::DataReader_ptr /*reader*/,
    const DDS::SampleRejectedStatus& /*status*/) {}

  virtual void on_data_available(DDS::DataReader_ptr reader)
  {
    MessageDataReader_var mdr = MessageDataReader::_narrow(reader);
    MessageSeq data;
    SampleInfoSeq infoseq;
    if (mdr->read_w_condition(data, infoseq, LENGTH_UNLIMITED, rc_) == RETCODE_ERROR) {
      cout << "ERROR: read_w_condition failed" << endl;
    }
  }

  virtual void on_sample_lost(DDS::DataReader_ptr /*reader*/,
                              const DDS::SampleLostStatus& /*status*/) {}
private:
  DDS::ReadCondition_var rc_;
};

bool test_setup(const DomainParticipant_var& dp,
  const MessageTypeSupport_var& ts, const Publisher_var& pub,
  const Subscriber_var& sub, const char* topicName, DataWriter_var& dw,
  DataReader_var& dr)
{
  CORBA::String_var typeName = ts->get_type_name();
  Topic_var topic = dp->create_topic(topicName, typeName,
                                     TOPIC_QOS_DEFAULT, 0,
                                     DEFAULT_STATUS_MASK);

  DataWriterQos dw_qos;
  pub->get_default_datawriter_qos(dw_qos);
  dw_qos.history.kind = KEEP_ALL_HISTORY_QOS;
  dw = pub->create_datawriter(topic, dw_qos, 0, DEFAULT_STATUS_MASK);

  DataReaderQos dr_qos;
  sub->get_default_datareader_qos(dr_qos);
  dr_qos.history.kind = KEEP_ALL_HISTORY_QOS;
  dr = sub->create_datareader(topic, dr_qos, 0, DEFAULT_STATUS_MASK);

  StatusCondition_var dw_sc = dw->get_statuscondition();
  dw_sc->set_enabled_statuses(PUBLICATION_MATCHED_STATUS);
  WaitSet_var ws = new WaitSet;
  ws->attach_condition(dw_sc);
  Duration_t infinite = {DURATION_INFINITE_SEC, DURATION_INFINITE_NSEC};
  ConditionSeq active;
  if (ws->wait(active, infinite) != DDS::RETCODE_OK) {
    return false;
  }
  ws->detach_condition(dw_sc);
  return true;
}

bool complex_test_setup(const DomainParticipant_var& dp,
  const MessageTypeSupport_var& ts, const Publisher_var& pub,
  const Subscriber_var& sub, const char* topicName, DataWriter_var& dw,
  DataReader_var& dr1, DataReader_var& dr2)
{
  CORBA::String_var typeName = ts->get_type_name();
  Topic_var topic = dp->create_topic(topicName, typeName,
                                     TOPIC_QOS_DEFAULT, 0,
                                     DEFAULT_STATUS_MASK);

  DataWriterQos dw_qos;
  pub->get_default_datawriter_qos(dw_qos);
  dw_qos.history.kind = KEEP_ALL_HISTORY_QOS;
  dw_qos.durability.kind = TRANSIENT_DURABILITY_QOS;
  dw_qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
  dw = pub->create_datawriter(topic, dw_qos, 0, DEFAULT_STATUS_MASK);

  DataReaderQos dr_qos;
  sub->get_default_datareader_qos(dr_qos);
  dr_qos.history.kind = KEEP_ALL_HISTORY_QOS;
  dr_qos.durability.kind = TRANSIENT_LOCAL_DURABILITY_QOS;
  dr_qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
  dr1 = sub->create_datareader(topic, dr_qos, 0, DEFAULT_STATUS_MASK);
  dr2 = sub->create_datareader(topic, dr_qos, 0, DEFAULT_STATUS_MASK);

  StatusCondition_var dw_sc = dw->get_statuscondition();
  dw_sc->set_enabled_statuses(PUBLICATION_MATCHED_STATUS);
  WaitSet_var ws = new WaitSet;
  ws->attach_condition(dw_sc);
  Duration_t infinite = {DURATION_INFINITE_SEC, DURATION_INFINITE_NSEC};
  ConditionSeq active;
  if (ws->wait(active, infinite) != DDS::RETCODE_OK) {
    return false;
  }
  ws->detach_condition(dw_sc);
  return true;
}

bool waitForSample(const DataReader_var& dr)
{
  ReadCondition_var dr_rc = dr->create_readcondition(ANY_SAMPLE_STATE,
    ANY_VIEW_STATE, ALIVE_INSTANCE_STATE);
  WaitSet_var ws = new WaitSet;
  ws->attach_condition(dr_rc);
  Duration_t infinite = {DURATION_INFINITE_SEC, DURATION_INFINITE_NSEC};
  ConditionSeq active;
  ReturnCode_t ret = ws->wait(active, infinite);
  ws->detach_condition(dr_rc);
  dr->delete_readcondition(dr_rc);
  if (ret != RETCODE_OK) {
    cout << "ERROR: wait(rc) failed" << endl;
    return false;
  }
  return true;
}

bool run_filtering_test(const DomainParticipant_var& dp,
  const MessageTypeSupport_var& ts, const Publisher_var& pub,
  const Subscriber_var& sub)
{
  DataWriter_var dw;
  DataReader_var dr;
  if (!test_setup(dp, ts, pub, sub, "MyTopic2", dw, dr)) return false;

  MessageDataWriter_var mdw = MessageDataWriter::_narrow(dw);
  Message sample;
  sample.key = 1;
  ReturnCode_t ret = mdw->write(sample, HANDLE_NIL);
  if (ret != RETCODE_OK) return false;
  if (!waitForSample(dr)) return false;

  ReadCondition_var dr_qc = dr->create_querycondition(ANY_SAMPLE_STATE,
    ANY_VIEW_STATE, ALIVE_INSTANCE_STATE, "key > 1", DDS::StringSeq());
  if (!dr_qc) {
    cout << "ERROR: failed to create QueryCondition" << endl;
    return false;
  }
  WaitSet_var ws = new WaitSet;
  ws->attach_condition(dr_qc);
  ConditionSeq active;
  Duration_t three_sec = {3, 0};
  ret = ws->wait(active, three_sec);
  // expect a timeout because the sample doesn't match the query string
  if (ret != RETCODE_TIMEOUT) {
    cout << "ERROR: wait(qc) should have timed out" << endl;
    return false;
  }
  ws->detach_condition(dr_qc);

  MessageDataReader_var mdr = MessageDataReader::_narrow(dr);
  MessageSeq data;
  SampleInfoSeq infoseq;
  ret = mdr->take_w_condition(data, infoseq, LENGTH_UNLIMITED, dr_qc);
  if (ret != RETCODE_NO_DATA) {
    cout << "ERROR: take_w_condition(qc) shouldn't have returned data" << endl;
    return false;
  }

  SampleInfo info;
  if (mdr->take_next_sample(sample, info) != RETCODE_OK) {
    cout << "ERROR: take_next_sample() should have returned data" << endl;
    return false;
  }

  sample.key = 2;
  if (mdw->write(sample, HANDLE_NIL) != RETCODE_OK) return false;
  if (!waitForSample(dr)) return false;

  ws->attach_condition(dr_qc);
  ret = ws->wait(active, three_sec);
  if (ret != RETCODE_OK) {
    cout << "ERROR: wait(qc) should not time out" << endl;
    return false;
  }
  ws->detach_condition(dr_qc);

  ret = mdr->take_w_condition(data, infoseq, LENGTH_UNLIMITED, dr_qc);
  if (ret != RETCODE_OK) {
    cout << "ERROR: take_w_condition(qc) should have returned data" << endl;
    return false;
  }

  dr->delete_readcondition(dr_qc);
  return true;
}

bool run_complex_filtering_test(const DomainParticipant_var& dp,
  const MessageTypeSupport_var& ts, const Publisher_var& pub,
  const Subscriber_var& sub)
{
  DataWriter_var dw;
  DataReader_var dr1;
  DataReader_var dr2;
  if (!complex_test_setup(dp, ts, pub, sub, "MyTopicComplex", dw, dr1, dr2)) return false;

  DDS::StringSeq params(2);
  params.length (2);
  params[0] = "5";
  params[1] = "10";
  QueryCondition_var dr_qc1 = dr1->create_querycondition(NOT_READ_SAMPLE_STATE,
    NEW_VIEW_STATE | NOT_NEW_VIEW_STATE, ANY_INSTANCE_STATE, "( (iteration > %0) AND (iteration < %1) )", params);
  if (!dr_qc1) {
    cout << "ERROR: failed to create QueryCondition 1" << endl;
    return false;
  }
  QueryCondition_var dr_qc2 = dr2->create_querycondition(NOT_READ_SAMPLE_STATE,
    NEW_VIEW_STATE | NOT_NEW_VIEW_STATE, ANY_INSTANCE_STATE, "( (iteration < %0) OR (iteration > %1) )", params);
  if (!dr_qc2) {
    cout << "ERROR: failed to create QueryCondition 2" << endl;
    return false;
  }

  DDS::DataReaderListener_var ml1 = new MessengerListener(dr_qc1);
  DDS::DataReaderListener_var ml2 = new MessengerListener(dr_qc2);
  MessageDataReader_var mdr1 = MessageDataReader::_narrow(dr1);
  MessageDataReader_var mdr2 = MessageDataReader::_narrow(dr2);
  mdr1->set_listener(ml1, DDS::DATA_AVAILABLE_STATUS);
  mdr2->set_listener(ml2, DDS::DATA_AVAILABLE_STATUS);
  MessageSeq data;
  SampleInfoSeq infoseq;
  ReturnCode_t ret = mdr1->take_w_condition(data, infoseq, LENGTH_UNLIMITED, dr_qc1);
  if (ret != RETCODE_NO_DATA) {
    cout << "ERROR: take_w_condition(qc1) shouldn't have returned data" << endl;
    return false;
  }
  ret = mdr2->take_w_condition(data, infoseq, LENGTH_UNLIMITED, dr_qc2);
  if (ret != RETCODE_NO_DATA) {
    cout << "ERROR: take_w_condition(qc2) shouldn't have returned data" << endl;
    return false;
  }

  MessageDataWriter_var mdw = MessageDataWriter::_narrow(dw);
  Message sample;
  for (CORBA::Long i = 0; i < 6; i++) {
    sample.key = i;
    if (mdw->register_instance(sample) == HANDLE_NIL) {
      cout << "ERROR: Registering instance failed" << endl;
      return false;
    }
  }

  for (CORBA::Long i = 0; i < 6; i++) {
    for (CORBA::Long j = 0; j < 15; j++) {
      sample.key = i;
      const DDS::InstanceHandle_t hnd = mdw->lookup_instance(sample);
      if (hnd == HANDLE_NIL) {
        cout << "ERROR: Lookup instance failed" << endl;
        return false;
      }
      sample.iteration = j;
      if (mdw->write(sample, hnd) != RETCODE_OK) return false;
    }
  }

  for (CORBA::Long i = 0; i < 6; i++) {
    sample.key = i;
    const DDS::InstanceHandle_t hnd = mdw->lookup_instance(sample);
    if (hnd == HANDLE_NIL) {
      cout << "ERROR: Lookup instance failed" << endl;
      return false;
    }
    if (mdw->dispose(sample, hnd) != RETCODE_OK) {
      cout << "ERROR: Dispose instance failed" << endl;
      return false;
    }
    if (mdw->unregister_instance(sample, hnd) != RETCODE_OK) {
      cout << "ERROR: Unregistering instance failed" << endl;
      return false;
    }
  }

  dr1->delete_readcondition(dr_qc1);
  dr2->delete_readcondition(dr_qc2);
  return true;
}

bool run_sorting_test(const DomainParticipant_var& dp,
  const MessageTypeSupport_var& ts, const Publisher_var& pub,
  const Subscriber_var& sub)
{
  DataWriter_var dw;
  DataReader_var dr;
  test_setup(dp, ts, pub, sub, "MyTopic", dw, dr);

  ReturnCode_t ret = RETCODE_OK;
  MessageDataWriter_var mdw = MessageDataWriter::_narrow(dw);
  Message sample;
  sample.key = 0;
  sample.name = "data_X";
  sample.nest.value = B;
  for (int i(0); i < 20; ++i, ++sample.key) {
    //replace the 'X' with a random letter
    sample.name.inout()[5] = static_cast<char>((rand() % 26) + 'A');
    ret = mdw->write(sample, HANDLE_NIL);
    if (ret != RETCODE_OK) return false;
    if (!(i % 4)) { //once in a while write more than 1 sample per instance
      Message sample2(sample);
      sample2.nest.value = A;
      sample2.name.inout()[5] = static_cast<char>((rand() % 26) + 'A');
      ret = mdw->write(sample2, HANDLE_NIL);
      if (ret != RETCODE_OK) return false;
      sample2.nest.value = C;
      ret = mdw->write(sample2, HANDLE_NIL);
      if (ret != RETCODE_OK) return false;
    }
  }

  DDS::StringSeq empty_query_params;
  ReadCondition_var dr_qc = dr->create_querycondition(ANY_SAMPLE_STATE,
    ANY_VIEW_STATE, ALIVE_INSTANCE_STATE, "ORDER BY name, nest.value",
    empty_query_params);
  if (!dr_qc) {
    cout << "ERROR: failed to create QueryCondition" << endl;
    return false;
  }
  WaitSet_var ws = new WaitSet;
  ws->attach_condition(dr_qc);
  MessageDataReader_var mdr = MessageDataReader::_narrow(dr);
  Duration_t five_seconds = {5, 0};
  bool passed = true, done = false;
  while (!done) {
    ConditionSeq active;
    ret = ws->wait(active, five_seconds);
    if (ret == RETCODE_TIMEOUT) {
      cout << "timeout ";
      done = true; //continue to try and read
    } else if (ret != RETCODE_OK) {
      passed = false;
      break;
    }
    cout << "wait returned" << endl;
    MessageSeq data;
    SampleInfoSeq info;
    ret = mdr->take_w_condition(data, info, LENGTH_UNLIMITED, dr_qc);
    if (ret == RETCODE_NO_DATA) {
      // fall-through
    } else if (ret != RETCODE_OK) {
      cout << "ERROR: take_w_condition returned " << ret << endl;
      passed = false;
      done = true;
    } else {
      char largest = 0;
      for (CORBA::ULong i(0); i < data.length(); ++i)  {
        cout << "Info:\tinstance_handle = " << info[i].instance_handle <<
          "\tsample_rank = " << info[i].sample_rank << '\n';
        if (info[i].valid_data) {
          cout << "Data:\tkey = " << data[i].key <<
            " \tname = " << data[i].name <<
            "\tnest.value = " << data[i].nest.value <<
            '\n';
          if (data[i].name[5] >= largest) {
            largest = data[i].name[5];
          } else {
            cout << "ERROR: data is not sorted for key: " <<
              data[i].key << endl;
            passed = false;
          }
        }
        cout << endl;
      }
    }
  }

  MessageSeq data;
  SampleInfoSeq info;
  ret = mdr->take_w_condition(data, info, LENGTH_UNLIMITED, dr_qc);
  if (ret != RETCODE_NO_DATA) {
    cout << "WARNING: there is still data in the reader\n";
  }

  ws->detach_condition(dr_qc);
  dr->delete_readcondition(dr_qc);
  return passed;
}

bool run_change_parameter_test(const DomainParticipant_var& dp,
  const MessageTypeSupport_var& ts, const Publisher_var& pub,
  const Subscriber_var& sub)
{
  DataWriter_var dw;
  DataReader_var dr;
  test_setup(dp, ts, pub, sub, "MyTopic3", dw, dr);

  MessageDataWriter_var mdw = MessageDataWriter::_narrow(dw);
  Message sample;
  sample.key = 3;
  ReturnCode_t ret = mdw->write(sample, HANDLE_NIL);
  if (ret != RETCODE_OK) return false;
  if (!waitForSample(dr)) return false;

  DDS::StringSeq params_empty(0);
  params_empty.length(0);
  ReadCondition_var dr_qc = dr->create_querycondition(ANY_SAMPLE_STATE,
    ANY_VIEW_STATE, ALIVE_INSTANCE_STATE, "key = %0", params_empty);
  if (dr_qc) {
    cout << "ERROR: Creating QueryCondition with 1 token and 0 parameters should have failed " << endl;
    return false;
  }

  DDS::StringSeq params_two(2);
  params_two.length(2);
  params_two[0] = "2";
  params_two[1] = "2";
  dr_qc = dr->create_querycondition(ANY_SAMPLE_STATE,
    ANY_VIEW_STATE, ALIVE_INSTANCE_STATE, "key = %0", params_two);
  if (dr_qc) {
    cout << "ERROR: Creating QueryCondition with 1 token and 2 parameters should have failed " << endl;
    return false;
  }

  DDS::StringSeq params(1);
  params.length(1);
  params[0] = "2";
  dr_qc = dr->create_querycondition(ANY_SAMPLE_STATE,
    ANY_VIEW_STATE, ALIVE_INSTANCE_STATE, "key = %0", params);
  if (!dr_qc) {
    cout << "ERROR: failed to create QueryCondition" << endl;
    return false;
  }

  QueryCondition_var query_cond = QueryCondition::_narrow(dr_qc);
  CORBA::String_var expr = query_cond->get_query_expression();
  if (std::string("key = %0") != expr.in()) {
    cout << "ERROR: get_query_expression() query expression should match " << endl;
    return false;
  }

  params = DDS::StringSeq();
  ret = query_cond->get_query_parameters(params);
  if (ret != RETCODE_OK) {
    cout << "ERROR: get_query_parameters() failed " << endl;
    return false;
  } else if (params.length() != 1 || std::string(params[0]) != "2") {
    cout << "ERROR: get_query_parameters() query parameters doesn't match " << endl;
    return false;
  }

  WaitSet_var ws = new WaitSet;
  ws->attach_condition(dr_qc);
  ConditionSeq active;
  Duration_t three_sec = {3, 0};
  ret = ws->wait(active, three_sec);
  // expect a timeout because the sample doesn't match the query string
  if (ret != RETCODE_TIMEOUT) {
    cout << "ERROR: wait(qc) should have timed out" << endl;
    return false;
  }
  ws->detach_condition(dr_qc);

  MessageDataReader_var mdr = MessageDataReader::_narrow(dr);
  MessageSeq data;
  SampleInfoSeq infoseq;
  ret = mdr->take_w_condition(data, infoseq, LENGTH_UNLIMITED, dr_qc);
  if (ret != RETCODE_NO_DATA) {
    cout << "ERROR: take_w_condition(qc) shouldn't have returned data" << endl;
    return false;
  }

  if (query_cond->set_query_parameters(params_empty) != RETCODE_ERROR) {
    cout << "ERROR: Setting 0 parameters for query condition with 1 token should have failed " << endl;
    return false;
  }

  if (query_cond->set_query_parameters(params_two) != RETCODE_ERROR) {
    cout << "ERROR: Setting 2 parameters for query condition with 1 token should have failed " << endl;
    return false;
  }

  params = DDS::StringSeq(1);
  params.length(1);
  params[0] = "3";
  ret = query_cond->set_query_parameters(params);

  params = DDS::StringSeq();
  ret = query_cond->get_query_parameters(params);
  if (ret != RETCODE_OK) {
    cout << "ERROR: get_query_parameters() failed " << endl;
    return false;
  } else if (params.length() != 1 || std::string(params[0]) != "3") {
    cout << "ERROR: get_query_parameters() query parameters doesn't match " << endl;
    return false;
  }

  ws->attach_condition(dr_qc);
  ret = ws->wait(active, three_sec);
  if (ret != RETCODE_OK) {
    cout << "ERROR: wait(qc) should not time out" << endl;
    return false;
  }
  ws->detach_condition(dr_qc);

  ret = mdr->take_w_condition(data, infoseq, LENGTH_UNLIMITED, dr_qc);
  if (ret != RETCODE_OK) {
    cout << "ERROR: take_w_condition(qc) should have returned data" << endl;
    return false;
  }

  dr->delete_readcondition(dr_qc);
  return true;
}

int run_test(int argc, ACE_TCHAR *argv[])
{
  DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);
  DomainParticipant_var dp =
    dpf->create_participant(23, PARTICIPANT_QOS_DEFAULT, 0,
                            DEFAULT_STATUS_MASK);
  MessageTypeSupport_var ts = new MessageTypeSupportImpl;
  ts->register_type(dp, "");

  Publisher_var pub = dp->create_publisher(PUBLISHER_QOS_DEFAULT, 0,
                                           DEFAULT_STATUS_MASK);

  Subscriber_var sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, 0,
                                             DEFAULT_STATUS_MASK);

  bool passed = run_sorting_test(dp, ts, pub, sub);
  passed &= run_filtering_test(dp, ts, pub, sub);
  passed &= run_change_parameter_test(dp, ts, pub, sub);
  passed &= run_complex_filtering_test(dp, ts, pub, sub);

  dp->delete_contained_entities();
  dpf->delete_participant(dp);
  return passed ? 0 : 1;
}


int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  int ret = 1;
  try
  {
    ret = run_test(argc, argv);
  }
  catch (const CORBA::BAD_PARAM& ex) {
    ex._tao_print_exception("Exception caught in QueryConditionTest.cpp:");
    return 1;
  }

  // cleanup
  TheServiceParticipant->shutdown ();
  ACE_Thread_Manager::instance()->wait();
  return ret;
}

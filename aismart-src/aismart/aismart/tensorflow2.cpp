//
// function/class implete here is relative of session. link to app directly.
//
#define GETTEXT_DOMAIN "rose-lib"

#include "filesystem.hpp"
#include "tensorflow2.hpp"
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"
#include "sdl_utils.hpp"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>

#include <tensorflow/core/framework/op_kernel.h>

#include <sstream>

namespace tensorflow2 {

class tnull_session_lock
{
public:
	tnull_session_lock(std::unique_ptr<tensorflow::Session>& session)
		: session_(session)
		, ok_(false)
	{}

	~tnull_session_lock()
	{
		if (!ok_) {
			session_.reset(nullptr);
		}
	}
	void set_ok(bool val) { ok_ = val; }

private:
	std::unique_ptr<tensorflow::Session>& session_;
	bool ok_;
};

tensorflow::Status load_model(const std::string& fname, std::unique_ptr<tensorflow::Session>& session) 
{
	tnull_session_lock lock(session);

	tensorflow::SessionOptions options;

	tensorflow::Session* session_pointer = nullptr;
	tensorflow::Status session_status = tensorflow::NewSession(options, &session_pointer);
	if (!session_status.ok()) {
		LOG(ERROR) << "Could not create TensorFlow Session: " << session_status;
		return session_status;
	}
	session.reset(session_pointer);

	size_t pos = fname.rfind(".pb");
	VALIDATE(pos != std::string::npos && pos + 3 == fname.size(), null_str);

	tensorflow::GraphDef tensorflow_graph;

	bool read_proto_succeeded = SDL_IsFile(fname.c_str());
	if (read_proto_succeeded) {
		read_proto_succeeded = tensorflow2::read_file_to_proto(fname, tensorflow_graph);
	}
	if (!read_proto_succeeded) {
		LOG(ERROR) << "Failed to load model proto from" << fname;
		return tensorflow::errors::NotFound(fname);
	}

	tensorflow::Status create_status = session->Create(tensorflow_graph);
	if (!create_status.ok()) {
		LOG(ERROR) << "Could not create TensorFlow Graph: " << create_status;
		return create_status;
	}

	lock.set_ok(true);
	return tensorflow::Status::OK();
}

tensorflow::Status load_model(const std::string& fname, std::pair<std::string, std::unique_ptr<tensorflow::Session> >& session2) 
{
	std::unique_ptr<tensorflow::Session>& session = session2.second;
	const std::string key = file_name(fname);
	if (key != session2.first) {
		tensorflow::Status s = tensorflow2::load_model(fname, session);
		if (!s.ok()) {
			session2.first.clear();
			return s;
		}
		session2.first = key;
	} else {
		VALIDATE(session2.second.get(), null_str);
	}
	return tensorflow::Status::OK();
}

// if fail return 0.
wchar_t inference_char(std::pair<std::string, std::unique_ptr<tensorflow::Session> >& current_session, const std::string& pb_path, cv::Mat& src2, uint32_t* used_ticks)
{
	tensorflow::Status s = tensorflow2::load_model(pb_path, current_session);
	if (!s.ok()) {
		std::stringstream err;
		err << "load model fail: " << s;
		return 0;
	}
	std::unique_ptr<tensorflow::Session>& session = current_session.second;

	// Read the label list
	std::vector<std::string> label_strings;
	
	cv::Mat src = get_adaption_ratio_mat(src2, 28, 28);

	int image_width = src.cols;
	int image_height = src.rows;
	int image_channels = 1;
	const int wanted_width = 28;
	const int wanted_height = 28;
	const int wanted_channels = 1;
	const float input_mean = 117.0f;
	const float input_std = 1.0f;
	VALIDATE(image_channels == wanted_channels, null_str);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels}));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	float* out = image_tensor_mapped.data();
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* in_row = src.ptr<uint8_t>(row);
		for (int x = 0; x < src.cols; x ++) {
			float* out_pixel = out + row * src.cols;
			if (in_row[x] == 255) {
				out_pixel[x] = 0;
			} else {
				out_pixel[x] = 1;
			}
		}
	}

	uint32_t start = SDL_GetTicks();

	wchar_t wch = 0;
	const int loops = 3;
	std::map<wchar_t, int> wchs;
	for (int loop = 0; loop < loops; loop ++) {
		std::vector<tensorflow::Tensor> outputs;
		tensorflow::Status run_status = session->Run({{"x-input", image_tensor}}, {"layer6-fc2/logit"}, {}, &outputs);
		if (!run_status.ok()) {
			std::stringstream err;
			err << "Running model failed: " << run_status;
			tensorflow::LogAllRegisteredKernels();
			return 0;
		}

		tensorflow::Tensor* output = &outputs[0];
		const Eigen::TensorMap<Eigen::Tensor<float, 1, Eigen::RowMajor>, Eigen::Aligned>& prediction = output->flat<float>();
	
		std::map<wchar_t, int>::iterator find;
		const long count = prediction.size();
		std::vector<float> floats;
		float max_value = INT_MIN;
		for (int i = 0; i < count; ++i) {
			const float value = prediction(i);
			if (value > max_value) {
				wch = '0' + i;
				max_value = value;
			}
			floats.push_back(value);
		}
		find = wchs.find(wch);
		if (find != wchs.end()) {
			find->second ++;
		} else {
			wchs.insert(std::make_pair(wch, 1));
		}
	}

	int max_loop = 0;
	for (std::map<wchar_t, int>::const_iterator it = wchs.begin(); it != wchs.end(); ++ it) {
		if (it->second > max_loop) {
			wch = it->first;
			max_loop = it->second;
		}
	}

	uint32_t end = SDL_GetTicks();
	if (used_ticks) {
		*used_ticks = end - start;
	}

	return wch;
}

}
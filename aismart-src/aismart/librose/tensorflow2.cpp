#define GETTEXT_DOMAIN "rose-lib"

#include "filesystem.hpp"
#include "tensorflow2.hpp"
#include "serialization/string_utils.hpp"
#include "wml_exception.hpp"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>

#include <tensorflow/core/framework/op_kernel.h>
#include <tensorflow/core/protobuf/meta_graph.pb.h>

#include <sstream>

namespace tensorflow2 {

std::string generate_link_function_name(const std::string& dir, const std::string& file)
{
	size_t pos = file.rfind(".");
	VALIDATE(pos != std::string::npos, null_str);
	std::stringstream ss;
	ss << "void tensorflow_link_" << dir << "_" << file.substr(0, pos) << "()";
	return ss.str();
}

std::string insert_link_function(const std::string& fullname)
{
	tfile file(fullname, GENERIC_WRITE, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);
	int fsize = file.read_2_data();

	const size_t pos1 = fullname.rfind("/");
	const size_t pos2 = fullname.rfind('/', pos1 - 1);
	const std::string dir = fullname.substr(pos2 + 1, pos1 - pos2 - 1);
	const std::string file2 = fullname.substr(pos1 + 1);

	std::string link_function = generate_link_function_name(dir, file2);
	if (!SDL_strstr(file.data, link_function.c_str())) {
		std::stringstream new_data;
		new_data << "\n" << link_function << " {}";
		int fsize2 = file.replace_span(fsize, 0, new_data.str().c_str(), new_data.str().size(), fsize);
		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize2);
	}
	return link_function;
}

class IfstreamInputStream : public ::google::protobuf::io::CopyingInputStream
{
public:
	explicit IfstreamInputStream(const std::string& file_name)
		: ifs_(file_name, GENERIC_READ, OPEN_EXISTING) {}
	~IfstreamInputStream() {}

	int Read(void* buffer, int size) {
		if (!ifs_.valid()) {
			return -1;
		}
		int result = posix_fread(ifs_.fp, buffer, size);
		return result;
	}

private:
	tfile ifs_;
};

bool read_file_to_proto(const std::string& file_name, ::google::protobuf::MessageLite& proto)
{
	::google::protobuf::io::CopyingInputStreamAdaptor stream(
		new IfstreamInputStream(file_name));
	stream.SetOwnsCopyingStream(true);
	// TODO(jiayq): the following coded stream is for debugging purposes to allow
	// one to parse arbitrarily large messages for MessageLite. One most likely
	// doesn't want to put protobufs larger than 64MB on Android, so we should
	// eventually remove this and quit loud when a large protobuf is passed in.
	::google::protobuf::io::CodedInputStream coded_stream(&stream);
	// Total bytes hard limit / warning limit are set to 1GB and 512MB
	// respectively.
	coded_stream.SetTotalBytesLimit(1024LL << 20, 512LL << 20);
	return proto.ParseFromCodedStream(&coded_stream);
}

}
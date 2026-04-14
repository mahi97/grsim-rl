# Build protobuf from source to avoid CRT mismatch with VarTypes on Windows.
# vcpkg protobuf v6+ depends on abseil which requires /MD (dynamic CRT),
# but VarTypes builds with /MT (static CRT). Building protobuf 3.6.1 from
# source with the same CRT as VarTypes avoids this.
include(BuildProtobuf)

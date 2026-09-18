// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/invitation.h"

#include <atomic>
#include <cstring>

namespace base {
namespace ipc {
namespace {

std::atomic<uint32_t> g_pair_seq{1};

std::wstring make_pair_path() {
  wchar_t name[96];
  swprintf_s(name, L"\\\\.\\pipe\\smartgis-ipc-pair-%u-%u",
             GetCurrentProcessId(), g_pair_seq.fetch_add(1));
  return name;
}

}  // namespace

PlatformChannel::PlatformChannel(HANDLE handle) : handle_(handle) {}

PlatformChannel::~PlatformChannel() {
  close();
}

PlatformChannel::PlatformChannel(PlatformChannel&& other) noexcept
    : handle_(other.handle_) {
  other.handle_ = INVALID_HANDLE_VALUE;
}

PlatformChannel& PlatformChannel::operator=(PlatformChannel&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    other.handle_ = INVALID_HANDLE_VALUE;
  }
  return *this;
}

bool PlatformChannel::is_valid() const {
  return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
}

HANDLE PlatformChannel::release() {
  const HANDLE h = handle_;
  handle_ = INVALID_HANDLE_VALUE;
  return h;
}

void PlatformChannel::close() {
  if (is_valid()) {
    CloseHandle(handle_);
  }
  handle_ = INVALID_HANDLE_VALUE;
}

bool PlatformChannel::create_pair(PlatformChannel* local,
                                  PlatformChannel* remote) {
  if (!local || !remote) {
    return false;
  }
  local->close();
  remote->close();
  const std::wstring path = make_pair_path();
  HANDLE server = CreateNamedPipeW(
      path.c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0,
      nullptr);
  if (server == INVALID_HANDLE_VALUE) {
    return false;
  }
  HANDLE client = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                              nullptr);
  if (client == INVALID_HANDLE_VALUE) {
    CloseHandle(server);
    return false;
  }
  OVERLAPPED ov = {};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ov.hEvent) {
    CloseHandle(client);
    CloseHandle(server);
    return false;
  }
  const BOOL ok = ConnectNamedPipe(server, &ov);
  if (!ok) {
    const DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) {
      WaitForSingleObject(ov.hEvent, 2000);
    } else if (err != ERROR_PIPE_CONNECTED) {
      CloseHandle(ov.hEvent);
      CloseHandle(client);
      CloseHandle(server);
      return false;
    }
  }
  CloseHandle(ov.hEvent);
  DWORD mode = PIPE_READMODE_BYTE;
  SetNamedPipeHandleState(server, &mode, nullptr, nullptr);
  SetNamedPipeHandleState(client, &mode, nullptr, nullptr);
  *local = PlatformChannel(server);
  *remote = PlatformChannel(client);
  return true;
}

PlatformChannel PlatformChannel::from_command_line(int argc, wchar_t** argv) {
  const size_t prefix = wcslen(k_channel_handle_switch);
  for (int i = 1; i < argc; ++i) {
    if (wcsncmp(argv[i], k_channel_handle_switch, prefix) == 0) {
      const unsigned long long v = _wcstoui64(argv[i] + prefix, nullptr, 10);
      return PlatformChannel(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(v)));
    }
  }
  return PlatformChannel();
}

OutgoingInvitation::OutgoingInvitation() {
  PlatformChannel local;
  if (PlatformChannel::create_pair(&local, &bootstrap_remote_)) {
    bootstrap_.adopt(local.release());
  }
}

OutgoingInvitation::~OutgoingInvitation() = default;

Channel OutgoingInvitation::attach(const std::string& name) {
  PlatformChannel local;
  PlatformChannel remote;
  Channel ch;
  if (!PlatformChannel::create_pair(&local, &remote)) {
    return ch;
  }
  ch.adopt(local.release());
  attached_.emplace_back(name, std::move(remote));
  return ch;
}

bool OutgoingInvitation::send_to(HANDLE target_process) {
  if (!bootstrap_.is_open() || !target_process) {
    return false;
  }
  InvitationBody body;
  body.version = 1;
  for (size_t i = 0; i < attached_.size() && i < 2; ++i) {
    PlatformHandle wrapped;
    if (!wrap_into(attached_[i].second.get(), target_process, &wrapped)) {
      return false;
    }
    InvitationPipeRec rec;
    rec.name = attached_[i].first;
    rec.handle_token = handle_to_token(wrapped);
    wrapped.release();
    if (i == 0) {
      body.pipe0 = std::move(rec);
    } else {
      body.pipe1 = std::move(rec);
    }
    attached_[i].second.close();
  }
  return bootstrap_.send_msg(k_msg_invitation, 0, body);
}

bool OutgoingInvitation::handoff_local(IncomingInvitation* incoming) {
  if (!incoming) {
    return false;
  }
  if (!send_to(GetCurrentProcess())) {
    return false;
  }
  *incoming = IncomingInvitation::accept(std::move(bootstrap_remote_));
  return true;
}

IncomingInvitation IncomingInvitation::accept(int argc,
                                              wchar_t** argv,
                                              uint32_t timeout_ms) {
  return accept(PlatformChannel::from_command_line(argc, argv), timeout_ms);
}

IncomingInvitation IncomingInvitation::accept(PlatformChannel channel,
                                              uint32_t timeout_ms) {
  IncomingInvitation out;
  if (!channel.is_valid()) {
    return out;
  }
  Channel boot;
  if (!boot.adopt(channel.release())) {
    return out;
  }
  Frame header{};
  std::vector<uint8_t> payload;
  if (!boot.recv(&header, &payload, timeout_ms)) {
    return out;
  }
  if (header.type != k_msg_invitation) {
    return out;
  }
  if (!decode(payload.data(), payload.size(), &out.body_)) {
    out.body_ = {};
  }
  return out;
}

Channel IncomingInvitation::extract(const std::string& name) {
  Channel ch;
  const InvitationPipeRec* rec = nullptr;
  if (body_.pipe0.name == name) {
    rec = &body_.pipe0;
  } else if (body_.pipe1.name == name) {
    rec = &body_.pipe1;
  }
  if (!rec || rec->handle_token == 0) {
    return ch;
  }
  // adopt() must take ownership; a temporary PlatformHandle would CloseHandle
  // the pipe before Channel stored it.
  PlatformHandle owned = handle_from_token(rec->handle_token);
  ch.adopt(owned.release());
  return ch;
}

bool launch_with_invitation(OutgoingInvitation* invitation,
                            const ChildLaunch& launch,
                            PROCESS_INFORMATION* pi) {
  if (!invitation || !pi || launch.exe.empty()) {
    return false;
  }
  HANDLE remote = invitation->bootstrap_remote();
  if (!remote || remote == INVALID_HANDLE_VALUE) {
    return false;
  }
  SetHandleInformation(remote, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

  SIZE_T attr_size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
  if (attr_size == 0) {
    return false;
  }
  std::vector<unsigned char> attr_buf(attr_size);
  auto* attr =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
  if (!InitializeProcThreadAttributeList(attr, 1, 0, &attr_size)) {
    return false;
  }
  HANDLE inherit_list[1] = {remote};
  if (!UpdateProcThreadAttribute(attr, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 inherit_list, sizeof(inherit_list), nullptr,
                                 nullptr)) {
    DeleteProcThreadAttributeList(attr);
    return false;
  }

  wchar_t cmd[2048];
  swprintf_s(cmd, L"\"%s\" %s %s%llu", launch.exe.c_str(),
             launch.extra_args.c_str(), k_channel_handle_switch,
             static_cast<unsigned long long>(
                 reinterpret_cast<uintptr_t>(remote)));

  STARTUPINFOEXW siex = {};
  siex.StartupInfo.cb = sizeof(siex);
  siex.lpAttributeList = attr;
  std::vector<wchar_t> cmd_buf(cmd, cmd + wcslen(cmd) + 1);
  const wchar_t* cwd = launch.cwd.empty() ? nullptr : launch.cwd.c_str();
  ZeroMemory(pi, sizeof(*pi));
  const BOOL ok =
      CreateProcessW(launch.exe.c_str(), cmd_buf.data(), nullptr, nullptr,
                     TRUE, CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                     nullptr, cwd, &siex.StartupInfo, pi);
  DeleteProcThreadAttributeList(attr);
  if (!ok) {
    return false;
  }
  if (launch.job) {
    AssignProcessToJobObject(launch.job, pi->hProcess);
  }
  if (!invitation->send_to(pi->hProcess)) {
    TerminateProcess(pi->hProcess, 1);
    CloseHandle(pi->hProcess);
    CloseHandle(pi->hThread);
    ZeroMemory(pi, sizeof(*pi));
    return false;
  }
  invitation->close_bootstrap_remote();
  return true;
}

}  // namespace ipc
}  // namespace base

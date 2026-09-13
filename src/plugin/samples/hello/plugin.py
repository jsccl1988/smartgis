def start(host):
    def _run(args):
        return True
    host.contribute_command("smartgis.sample_hello", "sample.hello", "Hello",
                            "tools", _run)


def stop():
    pass

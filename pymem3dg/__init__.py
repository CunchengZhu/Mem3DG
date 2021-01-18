def __bootstrap__():
    global __bootstrap__, __loader__, __file__
    import sys, importlib.util
    try: 
        import importlib.resources as pkg_resources
    except ImportError:
        # Try backported to PY<37 `importlib_resources`.
        import importlib_resources as pkg_resources
    # delete junk introduced
    __loader__ = None; del __bootstrap__, __loader__
    # get path to shared extension library
    with pkg_resources.path(__name__, "pymem3dg.cpython-38-x86_64-linux-gnu.so") as __file__:
        # load the module
        spec = importlib.util.spec_from_file_location(__name__, __file__)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
__bootstrap__()

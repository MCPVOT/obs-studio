# Characterization of the output protocol-list match in
# libobs/obs-output-protocols.c before its port: how many ';'-separated
# segments of a service output's protocols string equal the protocol. Builds
# the moved file on its own: no display and no libobs core, so it runs on
# every platform. The function is hidden in libobs, so this characterizes
# that C source; the test calls only it, so the port can run the same file
# against the Rust staticlib (as test_scene_bounds_place_rust does).
# test_output_protocols covers the caller through the real libobs.
# Target and test are declared in ../build/test-graph.json.

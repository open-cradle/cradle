set(input_file "${input_dir}/deploy_dir.h.in")
set(output_file "${output_dir}/deploy_dir.h")
if(${compiler_id} STREQUAL "MSVC")
    set(exe_ext ".exe")
else()
    set(exe_ext "")
endif()

get_filename_component(deploy_dir "${deployed_executable}" DIRECTORY)

message(VERBOSE "Generating ${output_file} from ${input_file}")

file(MAKE_DIRECTORY ${output_dir})
configure_file(${input_file} ${output_file} @ONLY)

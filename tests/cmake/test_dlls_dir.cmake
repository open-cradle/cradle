message(VERBOSE "Generating ${output_file} from ${input_file}")

get_filename_component(output_dir "${output_file}" DIRECTORY)
file(MAKE_DIRECTORY ${output_dir})
configure_file(${input_file} ${output_file} @ONLY)

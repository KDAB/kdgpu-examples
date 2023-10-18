#!/usr/bin/env ruby

require 'json'
require 'pp'

def expand_options(data)
  # Expand the options so that if no explicit options are specified we default
  # to options where the #define symbole is defined or not
  data[:options].each do |option|
    if !option.has_key?(:values)
      option[:values] = [:nil, :defined]
    end
    option[:count] = option[:values].size
  end
end

def extract_options(data, shader)
  shader_options = Hash.new
  shader_options[:options] = Array.new
  shader[:options].each do |option_index|
    shader_options[:options].push data[:options][option_index]
  end
  # STDERR.puts "Options for shader:"
  # STDERR.puts shader_options
  return shader_options
end

def find_bases(data)
  bases = Array.new(data[:options].size)
  (0..(data[:options].size - 1)).each do |index|
    bases[index] = data[:options][index][:count]
  end
  return bases
end

def calculate_steps(bases)
  step_count = bases[0]
  (1..(bases.size - 1)).each do |index|
    step_count *= bases[index]
  end
  return step_count
end

# Calculate the number for "index" in our variable-bases counting system
def calculate_digits(bases, index)
  digits = Array.new(bases.size, 0)
  base_index = digits.size - 1
  current_value = index
  while current_value != 0
    quotient, remainder = current_value.divmod(bases[base_index])
    digits[base_index] = remainder
    current_value = quotient
    base_index -= 1
  end
  return digits
end

def build_options_string(data, selected_options)
  str = ""
  selected_options.each_with_index do |selected_option, index|
    # Don't add anything if option is disabled
    next if selected_option == :nil

    # If we have the special :defined option, then we add a -D option
    if selected_option == :defined
      str += " -D#{data[:options][index][:define]}"
    else
      str += " -D#{data[:options][index][:define]}=#{selected_option}"
    end
  end
  return str.strip
end

def build_filename(shader, data, selected_options)
  str = File.basename(shader[:filename], File.extname(shader[:filename]))
  selected_options.each_with_index do |selected_option, index|
    # Don't add anything if option is disabled
    next if selected_option == :nil

    # If we have the special :defined option, then we add a section for that option
    if selected_option == :defined
      str += "_#{data[:options][index][:define].downcase}"
    else
      str += "_#{data[:options][index][:define].downcase}_#{selected_option.to_s}"
    end
  end
  str += File.extname(shader[:filename]) + ".spv"
  return str
end


# Load the configuration data and expand default options
if ARGV.size != 1
  puts "No filename specified."
  puts "  Usage: generate_shader_variants.rb <variants-config.json>"
  exit(1)
end

variants_filename = ARGV[0]
file = File.read(variants_filename)
data = JSON.parse(file, { symbolize_names: true })
expand_options(data)

# Prepare a hash to output as json at the end
output_data = Hash.new
output_data[:variants] = Array.new

data[:shaders].each do |shader|
  # STDERR.puts "Processing #{shader[:filename]}"

  # Copy over the options referenced by this shader to a local hash that we can operate on
  shader_options = extract_options(data, shader)

  # Create a "digits" array we can use for counting. Each element (digit) in the array
  # will correspond to an option in the loaded data configuration. The values each
  # digit can take are those specified in the "values" array for that option.
  #
  # The number of steps we need to take to count from "0" to the maximum value is the
  # product of the number of options for each "digit" (option).
  bases = find_bases(shader_options)
  # STDERR.puts "Bases = #{bases}"
  step_count = calculate_steps(bases)
  # STDERR.puts "There are #{step_count} combinations of options"

  # Count up through out range of options
  (0..(step_count - 1)).each do |index|
    digits = calculate_digits(bases, index)

    selected_options = Array.new(bases.size)
    (0..(bases.size - 1)).each do |digit_index|
      settings = data[:options][digit_index]
      setting_index = digits[digit_index]
      selected_options[digit_index] = settings[:values][setting_index]
    end

    # Construct the options to pass to glslangValidator
    defines = build_options_string(shader_options, selected_options)
    output_filename = build_filename(shader, shader_options, selected_options)

    # STDERR.puts "  Step #{index}: #{digits}, selected_options = #{selected_options}, defines = #{defines}, output_filename = #{output_filename}"

    variant = { input: shader[:filename], defines: defines, output: output_filename }
    output_data[:variants].push variant
  end

  # STDERR.puts ""
end

puts output_data.to_json

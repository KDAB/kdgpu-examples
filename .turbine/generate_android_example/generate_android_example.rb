require 'thor/group'
require 'active_support/inflector'
require 'fileutils'
require 'find'

module Turbine
  class GenerateAndroidExample < Thor::Group
    include Thor::Actions

    desc "Generates the skeleton of an Android project for an existing example"
    argument :name
    argument :example_path
    argument :friendly_name

    def self.source_root
      File.dirname(__FILE__)
    end

    def create_android_dir
      # Construct the correct path to the templates/android directory
      templates_dir = File.join(self.class.source_root, 'templates/android')

      # Iterate over all files in the templates/android/ directory, including subdirectories
      Find.find(templates_dir).each do |file|
        next if File.directory?(file) # Skip directories

        # Calculate the relative path for the destination
        relative_path = file.sub("#{templates_dir}/", '')
        destination_path = "#{example_path}/android/#{relative_path}"

        # Remove template extension, if we have one
        destination_path = destination_path.sub(/\.erb$/, '')

        # Apply the template function
        template(file, destination_path)

      end
    end

    def print_success_message
      say_status(:success, "Created Android project for example #{friendly_name}", :green)
      say_status(:info, "Please open the #{example_path}/android folder in Android Studio", :blue)
    end

    private

    def current_year
      @current_year = Time.now.year
    end

    def cpp_sources
      cpp_files = []

      # Scan the directory at example_path for .cpp files
      Find.find(example_path) do |path|
        # only consider cpp files, and avoid recursing in the android directory if this script has been run previously
        if path =~ /\.cpp$/ && !path.include?('/android/')
          # Extract the relative path and format it as "../cppFileName.cpp"
          cpp_file = path.sub("#{example_path}/", '')
          cpp_files << "../#{cpp_file}"
        end
      end

      # Join the file paths with spaces
      @cpp_sources = cpp_files.join(' ')      
    end
  end
end

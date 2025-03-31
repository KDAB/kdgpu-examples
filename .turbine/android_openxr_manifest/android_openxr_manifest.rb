require 'thor/group'
require 'active_support/inflector'
require 'fileutils'

module Turbine
  class AndroidOpenXRManifest < Thor::Group
    include Thor::Actions

    desc "Generates the manifest for an Android OpenXR project"
    argument :name
    argument :example_path

    def self.source_root
      File.dirname(__FILE__)
    end

    def create_android_dir
      template('templates/android/app/src/main/AndroidManifest.xml.erb', "#{example_path}/android/app/src/main/AndroidManifest.xml")
    end

    def print_success_message
      say_status(:success, "Created Android manifest for example #{name}", :green)
      say_status(:info, "Please open the #{example_path}/android folder in Android Studio", :blue)
    end

    private

    def current_year
      @current_year = Time.now.year
    end

  end
end

apply(from = "shared/gradle/android-settings.gradle")

rootProject.name = "KairoST"
include(":kairost", ":frontend")
project(":frontend").projectDir = file("shared/frontend")

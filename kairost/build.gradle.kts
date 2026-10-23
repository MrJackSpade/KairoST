plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

extra["kairoProduct"] = "kairost"
extra["kairoLabel"] = "KairoST"
apply(from = rootProject.file("shared/gradle/kairo-app.gradle"))

// EmuTOS, which the app ships for every model (third_party/emutos-1024k), in assets/emutos;
// MainActivity copies it out before the first game.
val emutosAssets = layout.buildDirectory.dir("generated/emutos-assets")
val stageEmutos = tasks.register<Copy>("stageEmutos") {
    from(rootProject.file("third_party/emutos-1024k/etos1024k.img"))
    into(emutosAssets.map { it.dir("emutos") })
}
android.sourceSets.getByName("main").assets.srcDir(emutosAssets)
tasks.named("preBuild") { dependsOn(stageEmutos) }

dependencies {
    testImplementation("junit:junit:4.13.2")
}

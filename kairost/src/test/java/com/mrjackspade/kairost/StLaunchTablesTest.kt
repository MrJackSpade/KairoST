package com.mrjackspade.kairost

import com.mrjackspade.kairo.LaunchTablesTest
import java.io.File

/** What KairoST builds as it launches (Kairo#125, KairoST#80). */
class StLaunchTablesTest : LaunchTablesTest() {
    override fun tables() = LaunchTables(listOf(StMedia.format), listOf(StKeys.layout),
        StFirmware.sets(File("tos")).map { it.second }, mapOf("controller bindings" to { StKeys.bindings }))
}

package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.FirmwareSet
import com.mrjackspade.kairo.frontend.R as FrontendR
import com.mrjackspade.kairo.frontend.UiText
import java.io.File

/** The firmware the Machine page lists, as the app's launch builds it (Kairo#125). */
object StFirmware {
    /** The player's TOS for each model, by the version its header names (StTos). */
    val tosParts = StTos.Model.entries.associateWith { model ->
        FirmwareSet.Part(model.file, UiText.res(R.string.tos_for_model, model.label),
            UiText.res(FrontendR.string.kairo_firmware_part, model.file, UiText.res(model.description)), setOf(model.file), false, sizes = setOf(model.bytes), identifies = { header -> StTos.rank(header, model) })
    }

    /** The TOS set, with its files in [directory]. */
    fun sets(directory: File): List<Pair<UiText, FirmwareSet>> = listOf(UiText.fixed("TOS") to FirmwareSet(directory, tosParts.values.toList()))
}

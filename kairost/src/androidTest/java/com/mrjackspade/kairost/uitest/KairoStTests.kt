package com.mrjackspade.kairost.uitest

import android.os.SystemClock
import com.mrjackspade.kairo.uitest.AppUnderTest
import com.mrjackspade.kairo.uitest.AppUnderTest.BiosFile
import com.mrjackspade.kairo.uitest.AppUnderTest.MediaSwap
import com.mrjackspade.kairo.frontend.GuestInputRecord
import com.mrjackspade.kairo.uitest.By
import com.mrjackspade.kairo.uitest.KairoUiTest
import com.mrjackspade.kairo.uitest.suites.ControllerTests
import com.mrjackspade.kairo.uitest.suites.FirstRunTests
import com.mrjackspade.kairo.uitest.suites.GameTests
import com.mrjackspade.kairo.uitest.suites.LaunchTests
import com.mrjackspade.kairo.uitest.suites.LibraryManagementTests
import com.mrjackspade.kairo.uitest.suites.LibraryTests
import com.mrjackspade.kairo.uitest.suites.MediaTests
import com.mrjackspade.kairo.uitest.suites.MenuTests
import com.mrjackspade.kairo.uitest.suites.SearchTests
import com.mrjackspade.kairo.uitest.suites.SettingsTests
import com.mrjackspade.kairo.uitest.suites.StateTests
import com.mrjackspade.kairo.uitest.suites.TouchTests
import com.mrjackspade.kairo.uitest.suites.WordingTests
import com.mrjackspade.kairost.StKeys
import org.junit.Test

/**
 * KairoST as the shared UI tests see it. Its test resources are two single-disk games in MSA
 * images and the player's TOS for each model (ST, STE, TT and Falcon); EmuTOS ships with the app.
 */
object KairoStApp : AppUnderTest() {
    override val name = "KairoST"
    override val brand = "KAIROST"
    override val biosTitle = "Atari ST BIOS"
    override val biosFiles = listOf(BiosFile("tos-st.img", false), BiosFile("tos-ste.img", false),
        BiosFile("tos-tt.img", false), BiosFile("tos-falcon.img", false))
    override val bootGame = "Goal!"
    override val bootGameFile = "games/GOAL.MSA"
    override val otherGame = "OOZE"
    override val searchTerm = "Goal"
    override val arrowKeys = listOf(StKeys.UP, StKeys.DOWN, StKeys.LEFT, StKeys.RIGHT)
    override val returnKey = StKeys.RETURN
    override val spaceKey = StKeys.SPACE
    override val mediaSwap = MediaSwap("Goal!", "Drive B", "OOZE")
}

class KairoStFirstRunTests : FirstRunTests() { override val app = KairoStApp }
class KairoStLibraryTests : LibraryTests() { override val app = KairoStApp }
class KairoStSearchTests : SearchTests() { override val app = KairoStApp }
class KairoStGameTests : GameTests() { override val app = KairoStApp }
class KairoStWordingTests : WordingTests() { override val app = KairoStApp }
class KairoStMediaTests : MediaTests() { override val app = KairoStApp }
class KairoStMenuTests : MenuTests() { override val app = KairoStApp }
class KairoStControllerTests : ControllerTests() { override val app = KairoStApp }
class KairoStTouchTests : TouchTests() { override val app = KairoStApp }
class KairoStLaunchTests : LaunchTests() { override val app = KairoStApp }
class KairoStLibraryManagementTests : LibraryManagementTests() { override val app = KairoStApp }
class KairoStSettingsTests : SettingsTests() { override val app = KairoStApp }
class KairoStStateTests : StateTests() { override val app = KairoStApp }

/** The Atari ST's own behavior. */
class StTests : KairoUiTest() {
    override val app = KairoStApp

    /**
     * KairoST#3: a touch on the lower screen's keyboard reaches the game at once. Players saw
     * seconds pass before a key registered; each of the first keys must arrive within half a second.
     */
    @Test
    fun aTouchKeyReachesTheGameAtOnce() {
        screens.completeFirstRun().open(app.bootGame).play()
        driver.waitFrames(120)
        val keys = driver.find(By.id("keyboard.key"))
        if (!keys.exists()) {
            driver.find(By.id("game.surface")).tap()
            keys.waitUntilShown()
        }
        for (key in keys.all().take(5)) {
            val code = key.key()!!.toInt()
            val mark = driver.inputMark()
            key.tap()
            val pressed = { event: GuestInputRecord.Event -> event.kind == "key" && event.code == code && event.down }
            driver.waitForInput("key $code", mark) { events -> events.any(pressed) }
            // From the finger going down to the key reaching the game, as the app saw both.
            val millis = driver.inputDelayAfterTouch(mark, pressed)
            driver.log.step("delay", "key $code reached the game after $millis ms")
            expect.that("key $code reached the game within 500 ms", millis < 500) { "$millis ms" }
        }
    }
}

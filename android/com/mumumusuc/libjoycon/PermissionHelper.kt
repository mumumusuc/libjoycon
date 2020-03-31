package com.mumumusuc.libjoycon

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.provider.Settings
import androidx.core.app.ActivityCompat
import java.lang.UnsupportedOperationException

class PermissionHelper {
    companion object {
        private const val RESULT_CODE_PERMISSION = 1

        fun check(activity: Activity): Boolean {
            return checkPermission(activity) && checkLocation(activity)
        }

        fun checkBluetooth(): Boolean {
            val adapter = BluetoothAdapter.getDefaultAdapter() ?: return false
            return adapter.isEnabled
        }

        fun requestBluetooth() {
            val adapter = BluetoothAdapter.getDefaultAdapter()
                ?: throw UnsupportedOperationException("seems that bluetooth is not available on this device.")
            adapter.enable()
        }

        fun checkPermission(activity: Activity): Boolean {
            if (Build.VERSION.SDK_INT < 23)
                return true
            return activity.checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }

        fun requestPermission(activity: Activity) {
            if (checkPermission(activity)) return
            ActivityCompat.requestPermissions(
                activity,
                arrayOf(Manifest.permission.ACCESS_FINE_LOCATION),
                RESULT_CODE_PERMISSION
            )
        }

        fun checkLocation(context: Context): Boolean {
            val location = context.getSystemService(Context.LOCATION_SERVICE) as LocationManager
            return location.isLocationEnabled()
        }

        fun requestLocation(context: Context) {
            if (checkLocation(context)) return
            context.startActivity(Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS))
        }
/*
    fun requestPermissionResult(requestCode: Int, resultCode: Int, data: Intent?) {
        when (requestCode) {
            RESULT_CODE_PERMISSION -> {
                if (resultCode == Activity.RESULT_OK) {
                    mReady = true
                } else {
                    mReady = false
                    debug("app cannot access bluetooth functions")
                }
            }
        }
    }
*/
    }
}
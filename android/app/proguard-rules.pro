# Retrofit + Gson
-keepattributes Signature
-keepattributes *Annotation*
-keep class com.qmanage.app.data.model.** { *; }
-keep class retrofit2.** { *; }
-keep interface retrofit2.** { *; }
-dontwarn retrofit2.**
-keep class com.google.gson.** { *; }
-dontwarn okhttp3.**
-dontwarn okio.**

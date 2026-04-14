import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

ThemeData buildSunrayV2Theme() {
  const bgCanvas = Color(0xFFF5F7F2);
  const bgSubtle = Color(0xFFEEF2EA);
  const surfaceCard = Color(0xFFFFFFFF);
  const textPrimary = Color(0xFF1F2A22);
  const textSecondary = Color(0xFF667267);
  const accentGreen = Color(0xFF2F7D4A);
  const accentLime = Color(0xFF7FBF4D);
  const dividerSoft = Color(0xFFDCE4D8);
  const dangerRed = Color(0xFFC6463B);

  final baseText = GoogleFonts.manropeTextTheme().copyWith(
    bodyMedium: const TextStyle(color: textPrimary, fontSize: 15),
    bodySmall: const TextStyle(color: textSecondary, fontSize: 12),
    titleLarge: const TextStyle(
      color: textPrimary,
      fontSize: 28,
      fontWeight: FontWeight.w700,
    ),
    titleMedium: const TextStyle(
      color: textPrimary,
      fontSize: 20,
      fontWeight: FontWeight.w700,
    ),
    titleSmall: const TextStyle(
      color: textPrimary,
      fontSize: 16,
      fontWeight: FontWeight.w700,
    ),
    labelLarge: const TextStyle(
      color: textPrimary,
      fontSize: 15,
      fontWeight: FontWeight.w700,
    ),
  );

  return ThemeData(
    useMaterial3: true,
    scaffoldBackgroundColor: bgCanvas,
    colorScheme: const ColorScheme.light(
      primary: accentGreen,
      secondary: accentLime,
      surface: surfaceCard,
      onSurface: textPrimary,
      error: dangerRed,
    ),
    textTheme: baseText,
    dividerColor: dividerSoft,
    appBarTheme: AppBarTheme(
      backgroundColor: bgCanvas,
      foregroundColor: textPrimary,
      elevation: 0,
      centerTitle: false,
      titleTextStyle: GoogleFonts.manrope(
        color: textPrimary,
        fontSize: 22,
        fontWeight: FontWeight.w700,
      ),
    ),
    cardTheme: CardThemeData(
      color: surfaceCard,
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(18),
        side: const BorderSide(color: dividerSoft),
      ),
    ),
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(
        backgroundColor: accentGreen,
        foregroundColor: Colors.white,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        minimumSize: const Size(0, 44),
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        textStyle: const TextStyle(fontSize: 14, fontWeight: FontWeight.w700),
      ),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: textPrimary,
        side: const BorderSide(color: dividerSoft),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        minimumSize: const Size(0, 44),
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        textStyle: const TextStyle(fontSize: 14, fontWeight: FontWeight.w700),
      ),
    ),
    chipTheme: ChipThemeData(
      backgroundColor: bgSubtle,
      selectedColor: const Color(0xFFDCEFD9),
      secondarySelectedColor: const Color(0xFFDCEFD9),
      side: const BorderSide(color: dividerSoft),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(999)),
      labelStyle: const TextStyle(
        color: textPrimary,
        fontWeight: FontWeight.w600,
      ),
    ),
  );
}

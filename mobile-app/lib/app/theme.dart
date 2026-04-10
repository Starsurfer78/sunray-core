import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

ThemeData buildSunrayTheme() {
  const deep = Color(0xFF060C17);
  const card = Color(0xFF0F1829);
  const border = Color(0xFF1E3A5F);
  const accent = Color(0xFF2563EB);
  const textPrimary = Color(0xFFE2E8F0);
  const textSecondary = Color(0xFF94A3B8);

  final scheme = ColorScheme.fromSeed(
    seedColor: accent,
    brightness: Brightness.dark,
    surface: card,
  );

  final baseTextTheme = GoogleFonts.interTextTheme().copyWith(
    bodyMedium: const TextStyle(color: textPrimary, fontSize: 14),
    bodySmall: const TextStyle(color: textSecondary, fontSize: 12),
    titleLarge: const TextStyle(color: textPrimary, fontSize: 22, fontWeight: FontWeight.w600),
    titleMedium: const TextStyle(color: textPrimary, fontSize: 16, fontWeight: FontWeight.w600),
    titleSmall: const TextStyle(color: textSecondary, fontSize: 13, fontWeight: FontWeight.w600),
    labelLarge: const TextStyle(color: textPrimary, fontSize: 14, fontWeight: FontWeight.w600),
  );

  return ThemeData(
    useMaterial3: true,
    colorScheme: scheme,
    scaffoldBackgroundColor: deep,
    cardColor: card,
    dividerColor: border,
    textTheme: baseTextTheme,
    appBarTheme: AppBarTheme(
      backgroundColor: deep,
      foregroundColor: textPrimary,
      elevation: 0,
      centerTitle: false,
      titleTextStyle: GoogleFonts.inter(
        color: textPrimary,
        fontSize: 20,
        fontWeight: FontWeight.w600,
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: const Color(0xFF08101D),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(14),
        borderSide: const BorderSide(color: border),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(14),
        borderSide: const BorderSide(color: border),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(14),
        borderSide: const BorderSide(color: accent),
      ),
    ),
    cardTheme: CardThemeData(
      color: card,
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(18),
        side: const BorderSide(color: border),
      ),
    ),
    navigationBarTheme: NavigationBarThemeData(
      backgroundColor: deep,
      indicatorColor: border,
      surfaceTintColor: Colors.transparent,
      labelBehavior: NavigationDestinationLabelBehavior.alwaysShow,
      labelTextStyle: WidgetStateProperty.resolveWith((states) {
        final isSelected = states.contains(WidgetState.selected);
        return TextStyle(
          color: isSelected ? accent : textSecondary,
          fontSize: 12,
          fontWeight: FontWeight.w500,
        );
      }),
      iconTheme: WidgetStateProperty.resolveWith((states) {
        final isSelected = states.contains(WidgetState.selected);
        return IconThemeData(color: isSelected ? accent : textSecondary);
      }),
    ),
  );
}

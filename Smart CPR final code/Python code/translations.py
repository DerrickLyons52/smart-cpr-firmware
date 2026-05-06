TRANSLATIONS = {
    "English": {
        "title": "SMART CPR",
        "welcome": "Welcome to SMART CPR",
        "start": "START",
        "back": "BACK",
        "view_stats": "VIEW STATS",
        "time": "Select Time",
        "custom": "Custom (min)",
        "countdown": ["3", "2", "1", "GO"],
        "depth": "DEPTH",
        "rate": "RATE",
        "error": "ERROR",
        "shallower": "SHALLOWER",
        "deeper": "DEEPER",
        "faster": "FASTER",
        "good": "GOOD",
        "slower": "SLOWER",
        "left": "MOVE LEFT",
        "right": "MOVE RIGHT",
        "up": "MOVE UP",
        "down": "MOVE DOWN",
        "full_recoil": "FULL RECOIL",
        "enter_name": "Enter your name",
        "user_stats": "User Stats",
        "no_data": "No data yet.",
        "delete_selected": "DELETE SELECTED SESSION",
        "session_complete": "Session complete",
        "pass": "PASS",
        "fail": "FAIL",
        "export_data": "EXPORT DATA",
        "warming_up": "System warming up...",
        "ready": "Ready",
        "not_connected": "System warming up...",
    },

    "Español": {
        "title": "SMART CPR",
        "welcome": "Bienvenido a SMART CPR",
        "start": "COMENZAR",
        "back": "VOLVER",
        "view_stats": "VER ESTADÍSTICAS",
        "time": "Seleccionar Tiempo",
        "custom": "Personalizado (min)",
        "countdown": ["3", "2", "1", "YA"],
        "depth": "PROFUNDIDAD",
        "rate": "VELOCIDAD",
        "error": "ERROR",
        "shallower": "MÁS SUPERFICIAL",
        "deeper": "MÁS PROFUNDO",
        "faster": "MÁS RÁPIDO",
        "good": "BUENO",
        "slower": "MÁS LENTO",
        "left": "MOVER IZQUIERDA",
        "right": "MOVER DERECHA",
        "up": "MOVER ARRIBA",
        "down": "MOVER ABAJO",
        "full_recoil": "REBOTAR COMPLETO",
        "enter_name": "Ingrese su nombre",
        "user_stats": "Estadísticas del usuario",
        "no_data": "No hay datos todavía.",
        "delete_selected": "BORRAR SESIÓN SELECCIONADA",
        "session_complete": "Sesión completa",
        "pass": "APROBADO",
        "fail": "FALLÓ",
        "export_data": "EXPORTAR DATOS",
        "warming_up": "Iniciando el sistema",
        "ready": "Listo",
        "not_connected": "Modo de simulación",
    },
}


def translate(language, key):
    """
    Return translated text for the selected language.
    Falls back to English, then to the key itself.
    """
    return TRANSLATIONS.get(language, TRANSLATIONS["English"]).get(
        key,
        TRANSLATIONS["English"].get(key, key)
    )


def available_languages():
    return list(TRANSLATIONS.keys())
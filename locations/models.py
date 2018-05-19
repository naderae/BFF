from django.db import models
from django.contrib.auth.models import User
# from django.contrib.gis.geos import Point
# from location_field.models.plain import PlainLocationField
from geoposition.fields import GeopositionField

class Location(models.Model):
    user = models.ForeignKey(User, on_delete=models.CASCADE)
    city = models.CharField(max_length=255)
    position = GeopositionField()

    def __str__(self):
        return self.city

    

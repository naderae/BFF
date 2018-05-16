from django.db import models
from django.contrib.auth.models import User

# Create your models here.

class Group(models.Model):
    name = models.CharField(max_length=255, unique=True)
    description = models.TextField(blank=True, default='')
    image = models.ImageField(upload_to='images/')
    members = models.ManyToManyField(User)


    def __str__(self):
        return self.name
